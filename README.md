# dotnet_linux_IO_API
Library for Linux, enables interaction with hardware from .NET / Mono

This library is a replacement for the Windows CE library ***coredll.dll***.

Run the Makefile on the development machine for your board.

```
make
```

Copy the library ***libfs_dotnet_io_api.so.0.1*** to ***/usr/lib***.
Then set a link in Mono to use this library, add following line to ***/etc/mono/config***:

```
<dllmap dll="coredll.dll" target="libfs_dotnet_io_api.so.0.1"/>
```

Also have a look at [NativeSPI-V1 for Linux](https://github.com/FSEmbedded/NativeSPI-V1_Linux), [NativeI2C for Linux](https://github.com/FSEmbedded/NativeI2C_Linux) and this [demo application](https://github.com/FSEmbedded/WinForms_On_Linux_InterfaceDemo).

# DISCLAIMER
We tested this software on our boards with some test applications. However, this code may contain errors and is not production ready yet!
SEE IT AS A PROOF OF CONCEPT AND USE AT YOUR OWN RISK.