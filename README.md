
# GS Server

GS Server is a lightweight service that integrates [Ghostscript](https://www.ghostscript.com/) to provide PDF/PCL file conversion and printing capabilities.  
It acts as a simple server layer around Ghostscript, enabling automated handling of print jobs and file processing.

## Prerequisites

Before building or running the service, make sure Ghostscript and development headers are installed:

```bash
sudo apt update
sudo apt install libgs-dev ghostscript
```

## License

This project is licensed under the [GNU Affero General Public License v3.0](LICENSE).