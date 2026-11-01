@echo off
echo Adding firewall rules for VideoLauncher Stream...
netsh advfirewall firewall add rule name="VideoLauncher Stream TCP" dir=in action=allow protocol=tcp localport=8080
netsh advfirewall firewall add rule name="VideoLauncher Stream UDP" dir=in action=allow protocol=udp localport=5557
netsh advfirewall firewall add rule name="VideoLauncher Stats UDP" dir=in action=allow protocol=udp localport=5556
echo Done!
pause
