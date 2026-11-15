#include "ExcelExport.h"
#include <QByteArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QString>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace payroll {
namespace {
// Minimal ZIP "stored" writer shared by XLSX and ODS packages.
// No compression dependency, executable, macro, or external link is included.
void U16(QByteArray& out, uint16_t value) {
    out.append(static_cast<char>(value & 255));
    out.append(static_cast<char>((value >> 8) & 255));
}
void U32(QByteArray& out, uint32_t value) {
    U16(out, static_cast<uint16_t>(value & 65535));
    U16(out, static_cast<uint16_t>(value >> 16));
}
uint32_t Size(QByteArray const& bytes) {
    if (static_cast<uint64_t>(bytes.size()) > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("Spreadsheet export is too large. Export fewer rows.");
    return static_cast<uint32_t>(bytes.size());
}
uint32_t Crc(QByteArray const& bytes) {
    uint32_t crc = 0xffffffffu;
    for (unsigned char byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
struct Part {
    QByteArray name, data;
};
QByteArray Zip(std::vector<Part> const& parts) {
    QByteArray out, directory;
    for (auto const& part : parts) {
        auto offset = Size(out), size = Size(part.data), crc = Crc(part.data);
        // DOS date 1980-01-01, time midnight; ASCII package paths.
        U32(out, 0x04034b50); U16(out, 20); U16(out, 0); U16(out, 0);
        U16(out, 0); U16(out, 33); U32(out, crc); U32(out, size); U32(out, size);
        U16(out, static_cast<uint16_t>(part.name.size())); U16(out, 0);
        out += part.name; out += part.data;
        U32(directory, 0x02014b50); U16(directory, 20); U16(directory, 20);
        U16(directory, 0); U16(directory, 0); U16(directory, 0); U16(directory, 33);
        U32(directory, crc); U32(directory, size); U32(directory, size);
        U16(directory, static_cast<uint16_t>(part.name.size())); U16(directory, 0);
        U16(directory, 0); U16(directory, 0); U16(directory, 0); U32(directory, 0);
        U32(directory, offset); directory += part.name;
    }
    auto offset = Size(out), size = Size(directory);
    out += directory;
    Size(out); // Reject ZIP64-sized packages instead of writing truncated offsets.
    U32(out, 0x06054b50); U16(out, 0); U16(out, 0);
    U16(out, static_cast<uint16_t>(parts.size()));
    U16(out, static_cast<uint16_t>(parts.size()));
    U32(out, size); U32(out, offset); U16(out, 0);
    return out;
}
QString Column(size_t column) {
    QString result;
    do {
        result.prepend(QChar(static_cast<ushort>('A' + column % 26)));
        column = column / 26;
        if (!column) return result;
        --column;
    } while (true);
}
QString CellText(std::wstring const& value, bool excelEscapes = true) {
    auto text = QString::fromStdWString(value);
    if (text.size() > 32767)
        throw std::runtime_error("A cell exceeds the export's 32,767-character limit. Shorten the text before exporting.");
    // XML 1.0 cannot contain most control characters or unpaired surrogates.
    for (qsizetype i = 0; i < text.size(); ++i) {
        auto c = text.at(i);
        if (c.isHighSurrogate()) {
            if (i + 1 >= text.size() || !text.at(i + 1).isLowSurrogate())
                throw std::runtime_error("A cell contains invalid Unicode text.");
            ++i;
        } else if (c.isLowSurrogate() || c.unicode() == 0xfffe || c.unicode() == 0xffff ||
                   (c.unicode() < 32 && c != QChar('\t') && c != QChar('\n') && c != QChar('\r'))) {
            throw std::runtime_error("A cell contains a control character the spreadsheet cannot display.");
        }
    }
    // Excel interprets _xHHHH_ escape sequences even in literal strings.
    if (excelEscapes)
        text.replace(QRegularExpression(QStringLiteral("_(?=x[0-9A-Fa-f]{4}_)")), QStringLiteral("_x005F_"));
    return text;
}
void SaveWorkbook(std::filesystem::path const& path, QByteArray const& workbook) {
    auto name = path.u8string();
    QSaveFile file(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    if (!file.open(QIODevice::WriteOnly) || file.write(workbook) != workbook.size() || !file.commit())
        throw std::runtime_error("Cannot save the spreadsheet. Close it in your spreadsheet application and check folder permissions and available disk space.");
}
void OdfText(QXmlStreamWriter& xml, QString const& text) {
    // ODF collapses plain XML whitespace; encode spaces, tabs and line breaks.
    xml.writeStartElement(QStringLiteral("text:p"));
    QString run;
    auto flush = [&] {
        if (!run.isEmpty()) { xml.writeCharacters(run); run.clear(); }
    };
    for (qsizetype i = 0; i < text.size(); ++i) {
        auto c = text.at(i);
        if (c == QChar(' ') || c == QChar('\t') || c == QChar('\r') || c == QChar('\n')) {
            flush();
            if (c == QChar(' ')) xml.writeEmptyElement(QStringLiteral("text:s"));
            else if (c == QChar('\t')) xml.writeEmptyElement(QStringLiteral("text:tab"));
            else {
                xml.writeEmptyElement(QStringLiteral("text:line-break"));
                if (c == QChar('\r') && i + 1 < text.size() && text.at(i + 1) == QChar('\n')) ++i;
            }
        } else run += c;
    }
    flush();
    xml.writeEndElement();
}
}
void ExportExcel(std::filesystem::path const& path,
                 std::vector<std::wstring> const& headers,
                 std::vector<std::vector<std::wstring>> const& rows,
                 std::vector<size_t> const& numericColumns) {
    if (headers.empty() || headers.size() > 16384 || rows.size() > 1048575)
        throw std::runtime_error("The export exceeds Excel's worksheet limits or has no columns.");
    for (auto column : numericColumns)
        if (column >= headers.size()) throw std::runtime_error("Invalid Excel numeric column.");
    QByteArray sheet;
    QXmlStreamWriter xml(&sheet);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("worksheet"));
    xml.writeDefaultNamespace(QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main"));
    xml.writeStartElement(QStringLiteral("sheetData"));
    QRegularExpression number(QStringLiteral("^-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?$"));
    auto line = [&](std::vector<std::wstring> const& values, size_t rowNumber, bool heading) {
        if (values.size() != headers.size()) throw std::runtime_error("Excel row has an unexpected number of columns.");
        xml.writeStartElement(QStringLiteral("row"));
        xml.writeAttribute(QStringLiteral("r"), QString::number(static_cast<qulonglong>(rowNumber)));
        for (size_t col = 0; col < values.size(); ++col) {
            auto text = CellText(values[col]);
            bool numeric = !heading && !text.isEmpty() &&
                std::find(numericColumns.begin(), numericColumns.end(), col) != numericColumns.end();
            xml.writeStartElement(QStringLiteral("c"));
            xml.writeAttribute(QStringLiteral("r"), Column(col) + QString::number(static_cast<qulonglong>(rowNumber)));
            xml.writeAttribute(QStringLiteral("t"), numeric ? QStringLiteral("n") : QStringLiteral("inlineStr"));
            if (numeric) {
                if (!number.match(text).hasMatch())
                    throw std::runtime_error("A numeric export cell is invalid. No workbook was saved.");
                xml.writeTextElement(QStringLiteral("v"), text);
            } else {
                // No formulas: user-entered =, +, -, @ text stays literal.
                xml.writeStartElement(QStringLiteral("is"));
                xml.writeStartElement(QStringLiteral("t"));
                xml.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
                xml.writeCharacters(text);
                xml.writeEndElement();
                xml.writeEndElement();
            }
            xml.writeEndElement();
        }
        xml.writeEndElement();
    };
    line(headers, 1, true);
    for (size_t i = 0; i < rows.size(); ++i) line(rows[i], i + 2, false);
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) throw std::runtime_error("Excel worksheet could not be generated.");
    auto workbook = Zip({
        {"[Content_Types].xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
</Types>)"},
        {"_rels/.rels", R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>)"},
        {"xl/workbook.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<sheets><sheet name="Payrollix Export" sheetId="1" r:id="rId1"/></sheets></workbook>)"},
        {"xl/_rels/workbook.xml.rels", R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
</Relationships>)"},
        {"xl/worksheets/sheet1.xml", sheet}
    });
    SaveWorkbook(path, workbook);
}
void ExportOds(std::filesystem::path const& path,
               std::vector<std::wstring> const& headers,
               std::vector<std::vector<std::wstring>> const& rows,
               std::vector<size_t> const& numericColumns) {
    // Use conservative worksheet limits that also work in modern Calc/Excel.
    if (headers.empty() || headers.size() > 16384 || rows.size() > 1048575)
        throw std::runtime_error("The export exceeds supported worksheet limits or has no columns.");
    for (auto column : numericColumns)
        if (column >= headers.size()) throw std::runtime_error("Invalid ODS numeric column.");
    QByteArray content;
    QXmlStreamWriter xml(&content);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("office:document-content"));
    xml.writeNamespace(QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:office:1.0"), QStringLiteral("office"));
    xml.writeNamespace(QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:table:1.0"), QStringLiteral("table"));
    xml.writeNamespace(QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:text:1.0"), QStringLiteral("text"));
    xml.writeAttribute(QStringLiteral("office:version"), QStringLiteral("1.2"));
    xml.writeStartElement(QStringLiteral("office:body"));
    xml.writeStartElement(QStringLiteral("office:spreadsheet"));
    xml.writeStartElement(QStringLiteral("table:table"));
    xml.writeAttribute(QStringLiteral("table:name"), QStringLiteral("Payrollix Export"));
    QRegularExpression number(QStringLiteral("^-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?$"));
    auto line = [&](std::vector<std::wstring> const& values, bool heading) {
        if (values.size() != headers.size()) throw std::runtime_error("ODS row has an unexpected number of columns.");
        xml.writeStartElement(QStringLiteral("table:table-row"));
        for (size_t col = 0; col < values.size(); ++col) {
            auto text = CellText(values[col], false);
            bool numeric = !heading && !text.isEmpty() &&
                std::find(numericColumns.begin(), numericColumns.end(), col) != numericColumns.end();
            xml.writeStartElement(QStringLiteral("table:table-cell"));
            xml.writeAttribute(QStringLiteral("office:value-type"),
                               numeric ? QStringLiteral("float") : QStringLiteral("string"));
            if (numeric) {
                if (!number.match(text).hasMatch())
                    throw std::runtime_error("A numeric export cell is invalid. No spreadsheet was saved.");
                xml.writeAttribute(QStringLiteral("office:value"), text);
            }
            // Explicit string cells, with no table:formula attribute, never execute formulas.
            OdfText(xml, text);
            xml.writeEndElement();
        }
        xml.writeEndElement();
    };
    line(headers, true);
    for (auto const& row : rows) line(row, false);
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) throw std::runtime_error("ODS spreadsheet could not be generated.");
    SaveWorkbook(path, Zip({
        // ODF requires this first, uncompressed, without a ZIP extra field or BOM.
        {"mimetype", "application/vnd.oasis.opendocument.spreadsheet"},
        {"content.xml", content},
        {"META-INF/manifest.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.2">
<manifest:file-entry manifest:full-path="/" manifest:version="1.2" manifest:media-type="application/vnd.oasis.opendocument.spreadsheet"/>
<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>
</manifest:manifest>)"}
    }));
}
} // namespace payroll
