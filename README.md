
# GS Server

## Overview
GS Server is a lightweight service for handling [Ghostscript](https://www.ghostscript.com/) conversion jobs and printing capabilities.  
It acts as a simple HTTP server around `Ghostscript`, enabling automated file conversion and optional printing to one or more network printers.

The service is designed to support flexible conversions such as:

- **PDF -> PCL** 
- **PDF -> PNG**
- **PDF -> JPG**

New formats can easily be added.

---

## How It works

1. The service receives an **HTTP POST** request containing a **PDF file** in the body.
2. The request `URL` includes **query parameters** defining `Ghostscript` options and output behavior.
3. GS Server saves the incoming `PDF` to disk, runs `Ghostscript`for conversion, and optionally sends the 
converted file to printer(s) via `TCP`

---

## Example Requests

### 1. Convert and Print

```
http://IP:PORT/?q&dNOPAUSE&dBATCH&dSAFER&sDEVICE=pxlmono&sOutputFile=FILE_NAME&print=IP1:PORT,IP2:PORT,...
```

### 2. Convert Only (no printing)

```
http://IP:PORT/?q&dNOPAUSE&dBATCH&dSAFER&sDEVICE=pxlmono&sOutputFile=FILE_NAME&print=
```

---

## Supported Conversions

The output format depends on the **sDEVICE** parameter:

- PCL formats (`.pcl`):
```
pxlmono, pxlcolor, pcl3, pclm, pclm8
```

- PNG formats (`.png`):
```
png16m, png16, png48, pngalpha, pnggray, pngmono
```

-JPEG formats (`.jpg`):
```
jpeg, jpeggray, jpegcmyk
```

---

## Configuration

All configuration is defined in `GSServer.properties`

- **filesDir**  -  Directory for input/output files
- **readonly**  -  Jobs are processed but not sent to printers (only logged)
- **disposal**  -  Both the source `.pdf` and the converted file are deleted after successful printing

---

## Prerequisites

Before building or running the service, make sure Ghostscript and development headers are installed:

```bash
sudo apt update
sudo apt install libgs-dev ghostscript
```

---


## License

This project is licensed under the [GNU Affero General Public License v3.0](LICENSE).