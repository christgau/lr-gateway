# Liveresults Gateway

A configurable tool that receives on-venue data/results (OVR) from an external
software component, operates on the data and emits HTTP requests to publish the
data.

# Data

Data is stored in named tables. Tables can also be loaded from files as well.
The follow a custom format that resemble Windows ini-Files. A table is
constituted by multiple rows in a ini file section. Multiple sections/tables
can be stored in a file. Every row is interpreted as a CSV line with a
semicolon (';') as delimiter between the items.

The protocol by which the data is received is proprietary. It is line based.
Each line is starts with an ASCII STX (2) and ends with ETX (3). Items in a
line are delimited with pipes ('|'). The first item in a row designates the
table where the remaining items will be added to. A zero appended to the table
name indicates the last line, a one indicates more lines to some. Tables with
an exclamation mark at their end can be considered a single line.

Examples:

 * `<STX>TBL1|Item 1|Item 2<ETX>` - non-last line of table TBL
 * `<STX>TBL0|Item 1|Item 2<ETX>` - last line of table TBL
 * `<STX>SLT!|Item 1|Item 2<ETX>` - only line of table SLT!

When a table has been completely received the *on table* event is triggered and
the configured actions for that table are processed. Further data requests can
be triggered as well.

The provided configuration file gives an overview of possible actions, HTTP
methods, and data modifications.

# HTTP requests

HTTP requests are build from the stored data which is converted to JSON either
by templates (key value pairs) or a single value. The HTTP method and the URL
are configurable as well. Several servers can be used to.

HTTPS, basic authentication, and client-side certificates are supported.

# Controlling the gateway

A control protocol, allows to pause, resume, terminate the gateway. In addition
configured actions can be triggered. The stored data can be shown as well. It
is also possible to selectively disable and enable HTTP targets. The `help`
command lists all available commands. The control protocol is useable
accessible via TCP on port 4422, but it depends on the actual configuration.

# Licence
CC BY-NC-SA 4.0

Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International

lr-gateway is a project by Steffen Christgau

You are free to:
 * Share — copy and redistribute the material in any medium or format
 * Adapt — remix, transform, and build upon the material

The licensor cannot revoke these freedoms as long as you follow the license
terms.

Under the following terms:
 * Attribution — You must give appropriate credit, provide a link to the
   license, and indicate if changes were made. You may do so in any reasonable
   manner, but not in any way that suggests the licensor endorses you or your
   use.

 * NonCommercial — You may not use the material for commercial purposes.

 * ShareAlike — If you remix, transform, or build upon the material, you must
   distribute your contributions under the same license as the original.

 * No additional restrictions — You may not apply legal terms or technological
   measures that legally restrict others from doing anything the license
   permits.
