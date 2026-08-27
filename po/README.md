# Project Eon translations

Project Eon reads UTF-8 GNU PO source files directly at runtime.  Keep one
`<language>.po` file per supported language code; region-specific files such
as `pt_BR.po` and `zh_CN.po` are selected for their language family too.
English strings are source text and deliberately have no PO file.

The built application first looks beside itself and in its installed shared
data directory; development builds also use this source directory.
