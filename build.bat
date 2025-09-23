rc resource.rc
cl BingTray.cpp resource.res user32.lib gdi32.lib shell32.lib ole32.lib urlmon.lib wininet.lib /EHsc /DUNICODE /D_UNICODE /Fe:BingTray.exe /link /SUBSYSTEM:WINDOWS