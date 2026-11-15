"""Validate workbooks produced by the application's real C++ exporter."""
import pathlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

NS = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
ODF = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
    "manifest": "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0",
}


def ods_sheet(path):
    mime = b"application/vnd.oasis.opendocument.spreadsheet"
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert archive.namelist() == ["mimetype", "content.xml", "META-INF/manifest.xml"]
        first = archive.infolist()[0]
        assert first.header_offset == 0 and first.compress_type == zipfile.ZIP_STORED
        assert first.extra == b"" and archive.read(first) == mime
        # Check the first local header too, not just the central directory.
        raw = path.read_bytes()
        assert int.from_bytes(raw[28:30], "little") == 0
        manifest = ET.fromstring(archive.read("META-INF/manifest.xml"))
        entries = {
            e.attrib[f"{{{ODF['manifest']}}}full-path"]: e.attrib[f"{{{ODF['manifest']}}}media-type"]
            for e in manifest
        }
        assert entries == {"/": mime.decode(), "content.xml": "text/xml"}
        content = ET.fromstring(archive.read("content.xml"))
        assert content.attrib[f"{{{ODF['office']}}}version"] == "1.2"
        return content


def ods_text(cell):
    paragraph = cell.find("text:p", ODF)
    result = paragraph.text or ""
    for child in paragraph:
        result += {
            f"{{{ODF['text']}}}s": " ",
            f"{{{ODF['text']}}}tab": "\t",
            f"{{{ODF['text']}}}line-break": "\n",
        }[child.tag]
        result += child.tail or ""
    return result


def sheet(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None, "ZIP CRC check failed"
        assert len(archive.namelist()) == 5
        for name in archive.namelist():
            ET.fromstring(archive.read(name))
        types = ET.fromstring(archive.read("[Content_Types].xml"))
        assert any(x.attrib.get("PartName") == "/xl/worksheets/sheet1.xml" for x in types)
        relations = ET.fromstring(archive.read("xl/_rels/workbook.xml.rels"))
        assert relations[0].attrib["Target"] == "worksheets/sheet1.xml"
        return ET.fromstring(archive.read("xl/worksheets/sheet1.xml"))


with tempfile.TemporaryDirectory(prefix="payrollix excel ") as directory:
    subprocess.run([sys.argv[1], directory], check=True)
    root = pathlib.Path(directory)
    xml = sheet(root / "unicode workbook.xlsx")
    rows = xml.findall("s:sheetData/s:row", NS)
    assert len(rows) == 5
    cells = {cell.attrib["r"]: cell for cell in xml.findall(".//s:c", NS)}

    def text(address):
        assert cells[address].attrib["t"] == "inlineStr"
        return cells[address].find("s:is/s:t", NS).text

    assert text("A2") == "Renée 😀"
    assert text("C2") == "0012345678"
    assert text("D2") == ' <&> "quoted"\nsecond line '
    assert text("A3") == "=1+1"
    assert text("A4") == "@SUM(A1)"
    assert text("D3") == "_x005F_x0041_"
    assert text("C3") == "+15551234567"
    assert text("A5") == "-text" and text("D5") == "+text"
    assert cells["B2"].attrib["t"] == "n"
    assert cells["B2"].find("s:v", NS).text == "1234.50"
    assert cells["B3"].find("s:v", NS).text == "-12.25"
    assert cells["B4"].find("s:v", NS).text == "0"
    assert cells["B5"].attrib["t"] == "inlineStr"
    assert not xml.findall(".//s:f", NS), "Unexpected executable formula"
    assert len(sheet(root / "empty.xlsx").findall("s:sheetData/s:row", NS)) == 1
    wide = sheet(root / "wide.xlsx")
    assert wide.find(".//s:c[@r='AA2']", NS) is not None
    assert wide.find(".//s:c[@r='AB2']", NS) is not None
    print("PASS: XLSX ZIP/XML, Unicode, literal text, numeric cells, empty and wide reports")
    ods = ods_sheet(root / "unicode workbook.ods")
    ods_rows = ods.findall(".//table:table-row", ODF)
    assert len(ods_rows) == 5
    grid = [r.findall("table:table-cell", ODF) for r in ods_rows]
    assert all(len(r) == 4 for r in grid)
    assert ods_text(grid[1][0]) == "Renée 😀"
    assert ods_text(grid[1][2]) == "0012345678"
    assert ods_text(grid[1][3]) == ' <&> "quoted"\nsecond line '
    assert ods_text(grid[2][0]) == "=1+1"
    assert ods_text(grid[2][2]) == "+15551234567"
    assert ods_text(grid[2][3]) == "_x0041_"
    assert ods_text(grid[3][0]) == "@SUM(A1)"
    assert ods_text(grid[3][3]) == "\tKeep whitespace\n"
    assert ods_text(grid[4][0]) == "-text" and ods_text(grid[4][3]) == "+text"
    for row, expected in [(1, "1234.50"), (2, "-12.25"), (3, "0")]:
        assert grid[row][1].attrib[f"{{{ODF['office']}}}value-type"] == "float"
        assert grid[row][1].attrib[f"{{{ODF['office']}}}value"] == expected
    assert grid[4][1].attrib[f"{{{ODF['office']}}}value-type"] == "string"
    assert all(
        f"{{{ODF['table']}}}formula" not in cell.attrib
        for row in grid for cell in row
    )
    assert len(ods_sheet(root / "empty.ods").findall(".//table:table-row", ODF)) == 1
    assert len(ods_sheet(root / "wide.ods").findall(".//table:table-row", ODF)[1]) == 28
    print("PASS: ODS package rules, manifest, numeric and literal cells, whitespace and empty reports")
