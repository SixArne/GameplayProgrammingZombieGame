start GPP_TEST_RELEASE.exe -x 30 -y 30 -s 69

start GPP_TEST_RELEASE.exe -x 1000 -y 30 -s 420

start GPP_TEST_RELEASE.exe -x 30 -y 540 -s 69420

start GPP_TEST_RELEASE.exe -x 1000 -y 540 -s 21
echo new ActiveXObject("WScript.Shell").AppActivate("GPP_TEST_RELEASE.exe"); > tmp.js
cscript //nologo tmp.js & del tmp.js