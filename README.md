fpassman is a fast password manager for Windows and Linux.

Features:

* Database encryption (AES-CBC with SHA256 key) & compression
* Graphical UI
* Console database accessor

## How to use

### Create a database

1. Start fpassman-gui.exe.  A new database is created automatically.
2. Add new entries (Entry->New...)
3. Save database to file (File->Save).  "Save database" window will open.
4. Enter target filename and password (twice).  Press Save.

### Open a database

1. Start fpassman-gui.exe.
2. Open database (File->Open...)
3. Enter filename and password.  Press Open.

### Search via console

1. Start cmd.exe
2. Execute `fpassman -d YOUR_DB -f YOUR_FILTER`.
3. Enter password.
4. A list of matched entries will be shown.

### Add entry via console

1. Start cmd.exe
2. Execute `fpassman -d YOUR_DB -a NAME USER PASS URL NOTES`.
3. Enter password.

### Edit entry via console

1. Start cmd.exe
2. Execute `fpassman -d YOUR_DB -e ID NAME USER PASS URL NOTES` where ID is the unique ID of the row you want to edit.
3. Enter password.

### Remove entry via console

1. Start cmd.exe
2. Execute `fpassman -d YOUR_DB -r ID` where ID is the unique ID of the row you want to delete.
3. Enter password.
