# Payrollix++ User Manual

Windows 11 edition | C++17 and Qt 6 Widgets | Visual Studio 2026

This manual is intended for company leadership, payroll operators and IT staff.
README.md and README.txt contain exactly the same UTF-8 text. Markdown headings
remain readable in a plain-text editor.

## 1. Overview and operating limits

Payrollix++ maintains local employee records, estimates payroll, stores processed
payslips and exports reports. It is a Windows desktop application, not a website.
The interface uses Qt 6 Widgets. Qt runtime libraries and its Windows platform
plugin are distributed with the application. Windows App SDK, WinUI and
C++/WinRT packages are no longer required. Qt's cross-platform capabilities
do not make this application cross-platform: its build and startup checks
restrict the program to Windows 11.

IMPORTANT: The inherited payroll formula is an estimate, not a compliant tax
engine. It assumes monthly salary, annualizes by multiplying by 12, applies
fixed federal brackets, uses a flat 5% state tax, and estimates Social Security
and Medicare. It does not use current withholding forms, filing status,
jurisdiction rules, year-to-date caps or certified tax tables. Benefits are
subtracted after estimating taxes, not treated as pre-tax benefits. Choosing
a weekly or biweekly schedule does NOT prorate monthly pay or change this model.
Have a qualified payroll professional validate calculations before business use.

The app does not transfer wages, file taxes, submit bank payments, send email,
provide approval workflows or synchronize several workstations. All signed-in
application users have access to the same features. The stored role field is
not a role-based permission system.

Workplace use: leadership can review staffing and estimated payroll costs;
operators can maintain records; IT can control deployment, backups and access.
Do not treat an exported estimate as a regulatory filing.

## 2. Requirements and Windows 11 exclusivity

Supported application platform: Windows 11 client editions, NT version 10.0,
build 22000 or newer, using the x64 build. Windows 10, Windows Server, macOS
and Linux are not supported. The executable checks the real OS version and
client/server edition at startup. There is no runtime switch to bypass it.
This build does not target ARM64 natively or 32-bit Windows.

For end users:
- A complete copy of the application output folder, including runtime files.
- Latest Microsoft Visual C++ v14 Redistributable, x64, at least as new as
  the MSVC build tools used to compile the application:
  https://aka.ms/vc14/vc_redist.x64.exe
- Permission to write to their Windows Local App Data folder.
- Microsoft Print to PDF enabled for PDF export, or a printer for paper output.
- Company-approved secure storage for exported payroll data and backups.

For building:
- Visual Studio 2026 (18.x) or its Build Tools, with MSVC v145 x64 C++ tools.
- Desktop development with C++ and Windows 11 SDK 10.0.26100.0.
- CMake 4.1 or newer, available through Visual Studio's C++ CMake tools.
- Qt 6.9 or newer, MSVC x64 desktop kit (for example Qt 6.9.3 msvc2022_64).
  The MSVC 2022 Qt binary kit is ABI-compatible with the v145 compiler.
  Do not use a MinGW, 32-bit or ARM64 kit.
- PowerShell 5.1 or newer.

Install Qt using an authorized Qt distribution and comply with its commercial
or applicable open-source license. No Qt or Windows App SDK packages are
silently downloaded by the build script. Windows CI uses Qt 6.9.3.

## 3. Build, distribution and launch

Installation and launch: open PowerShell in the project root, with the
prerequisites above installed, and run:

```powershell
cd PayRollix++
.\build.bat -Launch
```

The build automatically looks for Qt 6.9 or newer with the shared MSVC x64
desktop libraries, Widgets, Windows platform plugin and deployment tools.
It checks QTDIR, QT_ROOT_DIR, Qt6_DIR and CMAKE_PREFIX_PATH settings, Qt tools on
PATH, then common locations such as C:\Qt, other drive-root Qt folders,
your user Qt folder, Local App Data and Program Files Qt folders.
The newest compatible kit within the first successful search group is chosen,
and its path and version are printed before the build.

If no compatible kit is found, compilation stops with an error explaining
what to install and how to select a custom folder. Older versions, MinGW,
ARM64 and incomplete kits are rejected. Qt is not downloaded automatically.

For a custom location, this explicit PowerShell command remains supported:

```powershell
cd PayRollix++
.\build.ps1 -QtRoot C:\Qt\6.9.3\msvc2022_64 -Launch
```

To run through the Windows BAT launcher instead:

```powershell
cd PayRollix++
.\build.bat -QtRoot "C:\Qt\6.9.3\msvc2022_64" -Launch
```

Run either command block from the project root, not one after the other.
The BAT launcher permits script execution only for its PowerShell process;
it does not change saved execution policies, unblock files permanently, or
request administrator access. Organization-enforced policies still apply.
It forwards options to build.ps1 and returns its exit code. You can also
double-click build.bat to find Qt automatically; with no options,
it builds without launching the app and waits for a key before closing.

On a Windows 11 development workstation:
1. Open PowerShell in the PayRollix++ directory.
2. Run: .\build.ps1 (or .\build.bat) to discover Qt and build.
3. To build and launch, add: -Launch
4. For a debug build, add: -Configuration Debug
5. Optionally select a kit with -QtRoot or one of the environment settings above.
   An invalid explicit -QtRoot fails rather than silently choosing another kit.

The script locates Visual Studio 2026, configures CMake with the v145 toolset,
builds the application and runs Qt's windeployqt to copy its runtime files.
It does not install Visual Studio or Qt, or change PowerShell execution policy.
Follow company policy if scripts are blocked; do not disable protections globally.

For Visual Studio: open the PayRollix++ folder and choose the vs2026 CMake preset;
the same Qt discovery runs during configuration. Set PAYROLLIX_QT_ROOT in the
CMake cache if you need to select a specific kit. Or run build.ps1 first and open
the generated build\Payrollix.sln. The old checked-in WinUI solution and project
have been removed. Run build.ps1 after IDE builds to deploy runtime files,
or use CMake's install target into a separate deployment directory.

The project targets the v145 toolset; older Visual Studio installations are not
selected by the build script. The Windows CI workflow also explicitly uses
Visual Studio 2026 and builds the native tests with the same toolset.

Release output: bin\x64\Release\PayrollApp.exe
Distribute the ENTIRE Release directory, not the executable alone. The build
copies Qt runtime DLLs and the platforms\qwindows.dll plugin beside the executable.
Include the applicable Qt license notices and fulfill your distribution license
obligations; windeployqt alone is not a licensing compliance process. Install the x64
Visual C++ Redistributable separately on target computers. Run as a regular
Windows user; administrator privileges are not required for normal operation.

For IT: test the complete output on a clean Windows 11 workstation before
company rollout. Windows CI can compile using -SkipHostCheck on a build server;
that switch never bypasses the application's startup restriction.

## 4. First launch and existing databases

On first launch, choose one of these paths:
1. Enter a username and a password of at least 12 characters, confirm the
   password, select Create administrator, and then sign in.
2. Select Import existing database and choose an untouched copy of the previous
   payroll.db or a complete Payrollix++ SQLite backup. Sign in using that
   database's existing credentials.

New databases have no default administrator password. Old databases may still
contain their original default login. Change any default password immediately
under Account.

Live data is stored per Windows account:
%LOCALAPPDATA%\Payrollix\payroll.db

The previous working-directory database is not automatically moved or opened.
Keep an untouched backup before migration. The new app preserves existing table
names and adds a table for complete processed payslip details. The original
import source is not modified. Older databases with incomplete schemas are
rejected during import rather than silently replacing the current database.

Workplace use: pilot the migration with a copy first, reconcile record counts
and pay totals, and document who controls the original and migrated copies.
Different Windows accounts do not automatically share this database.

## 5. Sign-in, passwords and sign-out

What it does: protects access to the local application interface and records
the signed-in user on new audit entries.

How to use:
1. Enter your username and password on the welcome screen and select Sign in.
2. After five unsuccessful attempts, wait 30 seconds before retrying.
3. Select Account to change your password. Supply your current password and
   enter the new password twice.
4. Select Sign out before leaving an unattended workstation.

Existing Qt SHA-256 password hashes are recognized and upgraded to salted
PBKDF2-SHA256 after successful login. New hashes use Windows BCrypt and 600,000
iterations. There is no online password recovery or central account directory.

Usefulness: reduces casual access and attributes application actions to a user.
Workplace application: combine it with Windows screen locking, restricted
folder permissions and controlled workstation access. The database is not
encrypted by this app; a person who can edit it directly can bypass its UI.

## 6. Dashboard

What it does: displays the count of active employees, current gross pay
including overtime, estimated net payroll, the latest open pay period and
recent audit activity.

How to use: select Dashboard, review totals and pay date, and select Refresh
after making changes elsewhere.

Usefulness: gives management a quick current staffing and cost overview.
Workplace application: use it in preparation meetings and exception reviews,
not as a historical pay-run ledger. The totals reflect current employee
records, include active records only, and are not bank payment confirmations.

## 7. Adding employees and estimating payroll

What it does: creates an employee record and calculates estimated taxes,
overtime, deductions and net pay.

How to use:
1. Select Add employee.
2. Enter the required full name and a valid hire date as YYYY-MM-DD.
3. Enter job title, company, contact information, biography and internal notes.
4. Choose a department or leave Unassigned; choose employment status.
5. Enter monthly base salary, hourly rate for overtime, overtime hours and
   multiplier. The multiplier must be between 1 and 5.
6. Enter health insurance, 401(k) and other deduction amounts.
7. Select Calculate and save. Review the estimated net pay confirmation.
8. Use Clear form to discard unsaved entries, or Cancel to return to Employees.

Regular hours are reference information only. They do not automatically
multiply the hourly rate to produce salary. All money is in USD. Inputs must
be finite, nonnegative values; the app rejects invalid amounts.

Usefulness: consolidates personnel and compensation information.
Workplace application: reconcile inputs against authorized compensation
documents, keep sensitive notes minimal, and independently review the estimate
before processing payroll. Saving an employee is not a pay run or a payment.

## 8. Employee directory, search and editing

What it does: lists employees and their current role, department, gross pay,
estimated net pay and employment status.

How to use:
1. Select Employees.
2. Enter part of a name, role or company; choose department and status filters.
3. Select Search / refresh.
4. Select a row and choose Edit. Change fields and select Calculate and save.
5. Choose History or Payslip for the selected employee's related records.

Usefulness: finds records quickly and keeps compensation information current.
Workplace application: use department filters for manager reviews. Changes to
salary, role and status automatically create employment history events.
Editing current employee data does not rewrite complete processed snapshots.

## 9. Employee status and deletion

What it does: distinguishes active, inactive and terminated staff. Batch
payroll includes active staff only; the dashboard excludes inactive staff.

How to use: open Employees, select the employee, choose Edit, change Employment
status and save. To delete a record, select it in Employees and choose Delete,
then review the confirmation.

Deletion removes the employee and their employment notes. Employees with
processed payslips cannot be deleted; terminate them instead. A department
manager reference is cleared if it points to a deleted employee.

Usefulness: supports departures and temporary inactivity without losing pay
history. Workplace application: follow approved retention rules and create
a backup before a permitted deletion. An audit record is not a substitute for
retaining legally required employment documentation.

## 10. Departments

What it does: groups employees for directory filters and reports.

How to use:
1. Select Departments.
2. Enter a name and description, then select Save department to create one.
3. Select a department row to populate its fields, edit them and save.
4. Select New / clear before creating another department.
5. To delete one, select its row and choose Delete selected.

Departments assigned to employees cannot be deleted. Reassign those employees
through Edit employee first.

Usefulness: organizes payroll by a recognizable company structure.
Workplace application: use consistent department names aligned with internal
cost-center reporting. This app does not assign accounting ledger codes.

## 11. Pay periods

What it does: labels payroll snapshots with a schedule, date range and pay date,
and controls whether further processing is allowed.

How to use:
1. Select Pay periods and enter a period name.
2. Choose Weekly, Bi-Weekly, Semi-Monthly or Monthly.
3. Enter start, end and pay dates in YYYY-MM-DD format.
4. Ensure start <= end <= pay date and select Add period.
5. Select a row and choose Close selected to prevent additional processing,
   or Reopen selected to allow processing again.
6. Delete selected removes an unused period after confirmation. Periods
   referenced by payroll records cannot be deleted.

Usefulness: keeps payroll runs organized and prevents accidental processing
against closed periods. Workplace application: close a period after review.
Schedule selection is a label, NOT an automatic salary proration or tax rule.

## 12. Batch payroll processing

What it does: recalculates selected active employees and saves a processed
payslip snapshot for each employee in the selected open period.

How to use:
1. Review employee pay inputs and create an open period first.
2. Select Batch payroll and choose the open period.
3. Select employee rows individually, or choose Select all. Clear selection
   removes the selection.
4. Select Process selected, review the warning and confirm.
5. Review the confirmation and inspect payslips from Employees.

The entire batch is transactional: a failure rolls back every employee in
that batch. Processing an employee twice in the same period is rejected.
Reopening a period does not permit duplicate processing of the same employee.
There is no automatic reversal/correction workflow for a processed snapshot.

Usefulness: reduces repeated processing and preserves the figures reviewed
at the time of a run. Workplace application: obtain approval outside the app
before confirming; independently reconcile the totals. Processing does not
send funds or produce bank instructions. Non-monthly runs are not prorated.

## 13. Employment history

What it does: records promotions, salary changes, transfers, reviews and other
events. Salary, role and status edits also create automatic events.

How to use:
1. Select an employee in Employees and choose History.
2. Choose an event type and enter previous/new values, date and notes.
3. Select Record event.
4. To remove an incorrect event, select it and choose Delete selected event,
   then confirm. The removal is audited.

Recording a manual event does NOT change the actual salary, department or
role. Use Edit employee to make those changes.

Usefulness: explains why employee records changed.
Workplace application: retain approved references to personnel decisions,
avoid unnecessary private information in notes, and apply HR retention policy.

## 14. Payslip preview, PDF, HTML and printing

What it does: shows a current estimate or a saved processed payslip with
earnings, deductions and net pay.

How to use:
1. In Employees, select a row and choose Payslip.
2. Choose Current estimate or a processed pay period and select Generate.
3. Review the preview. Changing the dropdown alone does not regenerate it.
4. Select Export PDF, select a destination and save. This uses Windows'
   Microsoft Print to PDF printer; it must be enabled.
5. Select Export HTML to save a browser-readable UTF-8 document.
6. Select Print for the Windows print dialog and a physical printer.

Exports contain the generated preview, not an ungenerated dropdown selection.
The current estimate is explicitly labeled and is not a processed payslip.
New processed snapshots retain the saved employee identity and deduction
breakdown. Old Qt payslips may contain only gross/net and four tax values;
missing historical details are identified rather than filled from today's data.
Printing wraps long text and paginates; HTML escapes employee-entered text.

Usefulness: supplies a reviewable employee pay statement.
Workplace application: check the selected period and distribute via approved
secure channels. The app does not email statements or certify legal payslip
content requirements.

## 15. Reports and CSV export

What it does: produces four reports from current employee records:
- Payroll Summary: employee, department, gross including overtime, deductions,
  estimated net and status.
- Department Report: employee count, gross, net and average base salary.
- Employee List: role, department, contact details, hire date and status.
- Deductions Summary: federal/state tax, Social Security, Medicare and benefits.

How to use:
1. Select Reports and choose a report type and optional department.
2. Optionally enter Updated from / Updated to dates as YYYY-MM-DD.
3. Select Generate report and review the rows and totals.
4. Select Export generated report to CSV and choose a destination.
5. Open the CSV in a spreadsheet or import it into an approved system.

All report filters apply to the generated report. Dates filter the employee
record's last update date in UTC, not a historical pay period. Leave dates blank
for all current records. Reports include all statuses unless their department
or update-date filters exclude them. Generate again after changing filters.
CSV exports exactly the generated rows, even if filters are edited afterwards.

CSV is UTF-8 with a byte-order mark, quoted fields, escaped double quotes and
Windows line endings. Values that could be interpreted as spreadsheet formulas
are prefixed with an apostrophe. This may affect downstream import expectations.

Usefulness: supports staffing reviews, deduction checks and cost analysis.
Workplace application: reconcile to the approved pay run and map columns
explicitly when importing. CSV is not a vendor-specific accounting integration.

## 16. Audit log

What it does: records application logins, record changes, processing, generated
reports, exports and backup/restore actions.

How to use:
1. Select Audit log.
2. Choose an action and optional From / To dates.
3. Select Apply filters / refresh to display matching records.
4. Review UTC timestamp, username, action, table, record ID and details.
5. Select Export filtered log to CSV to save the displayed result.

Usefulness: helps IT and managers investigate application actions.
Workplace application: review unusual changes and store approved exports in
controlled storage. Older actions without a user are labeled System / legacy.
The local log can be changed by someone with database access; it is not
tamper-evident, immutable or a guarantee of compliance. Restoring a backup also
restores its historical log, then adds a new restore entry.

## 17. Database backup and restore

What it does: copies a consistent SQLite database with its users and records.

Creating a backup:
1. Select Backup and restore.
2. Select Create backup and choose a .db destination.
3. Wait for confirmation and retain the file in approved secure storage.

Restoring:
1. Select Restore from backup and choose an existing Payrollix++ .db backup.
2. Read the replacement warning and confirm only if authorized.
3. The app checks integrity and expected columns before copying the data.
4. A safety backup is saved next to the live database with a
   .before-restore-...db suffix.
5. The selected backup replaces current users, passwords and payroll records.
6. Sign in using the restored database's credentials.

The source backup is not modified. Choosing the live database as the source
or destination is rejected. Backup/restore uses SQLite's backup API rather
than copying an open database file.

Usefulness: provides workstation recovery and controlled migration.
Workplace application: schedule backups outside the app, retain multiple
generations, test restoration on a separate account/computer and limit who
may restore production information. Backups are not encrypted by this app.

## 18. Troubleshooting and IT verification

- Unsupported OS: use Windows 11 client build 22000 or newer, not Server or
  Windows 10. Compatibility mode is not a supported workaround.
- Missing runtime DLLs: deploy the full Release directory and install the x64
  Visual C++ Redistributable. Copying only PayrollApp.exe is insufficient.
- MSBuild or SDK missing: install the prerequisites and SDK version listed
  above. Use build.ps1, or configure CMake with the Visual Studio 2026 preset.
- Qt not found: install Qt 6.9+ with the MSVC x64 desktop kit and retry.
  If Qt is in a custom location, pass -QtRoot pointing to that kit folder,
  not its bin folder. The error also appears if only incompatible or incomplete
  kits are installed. The build prints the selected kit when discovery succeeds.
  Remove an obsolete generated build directory when switching toolchains.
- Qt platform plugin missing: run build.ps1 again and deploy the entire output
  folder, including platforms\qwindows.dll; do not copy only the executable.
- PDF unavailable: enable Microsoft Print to PDF in Windows Features or
  export HTML and use an approved external print workflow.
- Cannot open data or export: check folder permissions, available disk space
  and whether the destination file is locked by another program.
- No employees after migration: the app does not discover the old working
  directory automatically. Import a backup during setup, or restore it from
  Backup and restore after signing in.
- Password lost: there is no reset service. Use an authorized backup or seek
  help from your organization's designated application administrator.
- Locked database: close other tools editing the database and retry. Do not
  place the live database on an unsupported shared/network drive.
- Unexpected totals: review monthly base pay, overtime inputs, employee status,
  report update-date filters and the estimator limits in section 1.

Verification commands from PayRollix++:
- Project/manual checks: python tests/check_project.py
- Portable core checks: cmake -S tests -B build-tests
  then cmake --build build-tests --config Release
  then ctest --test-dir build-tests -C Release --output-on-failure

On Linux, core tests need a C++17 compiler, CMake and SQLite development files.
Their test-only password provider does not verify Windows cryptography.
On Windows, core tests link real BCrypt and verify native password hashing.
The GitHub workflow builds the Qt application and runs native core tests
on a Windows compile host. It does not exercise the interactive interface.

This Linux workspace cannot link or run the Windows application. It can compile
Qt interface objects with -DPAYROLLIX_CHECK_QT_UI=ON in the tests project;
that check does not produce a non-Windows executable or test native dialogs.
Before rollout, verify sign-in, employee creation/editing, department and period
management, payroll snapshots, CSV, Unicode payslips, PDF/printing, backup and
restore on Windows 11. Test dark/light themes, keyboard use, display scaling
and resize behavior. Also verify startup rejection on an unsupported OS.

For support, provide the Windows version, build configuration and exact error,
with company information removed. Never send payroll databases, passwords or
unredacted employee data through an unapproved support channel.
