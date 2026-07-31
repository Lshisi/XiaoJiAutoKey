# XiaoJi HID

`XiaoJi HID` is the XiaoJi keyboard, mouse and consumer-control driver. Its stable internal
service and package identifier is `XiaoJiVhid`.
It is a root-enumerated KMDF source driver backed by the Windows Virtual HID Framework.

## Build requirements

- Visual Studio 2022 with Desktop C++ tools
- Windows Driver Kit matching the installed Windows SDK
- x64 driver build tools and `VhfKm.lib`

Build `XiaoJiVhid.sln` as `Release|x64`. The package output is written to
`package/Release` and contains the SYS, INF and generated CAT files.

Development machines may use a test certificate and test-signing mode. Production packages
must be submitted for Microsoft signing and must not ship with a private test certificate.

`scripts/package-release.ps1` rejects test-signed native drivers by default. After Microsoft
attestation or WHQL signing, place the signed SYS, INF and CAT files in `package/Release` and
run the release script normally. For an internal development-only archive, pass
`-AllowTestSignedNativeDriver`; that switch rebuilds and packages the WDK test-signed output.

## Runtime contract

The shared device interface GUID, IOCTL definitions and fixed-width request structures live in
`../shared/XiaoJiVhidProtocol.h`. The desktop runtime acquires an exclusive lease, sends a
heartbeat every 500 ms and releases all reports when its handle closes. The kernel watchdog
revokes a stale lease and releases every key and mouse button after 2500 ms.
