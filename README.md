
1,现状
3D OPENGL 加速, WIFI, 屏幕背光关闭 都正常，系统稳定性良好。

intel 的 ATOM 显卡驱动有个vblank BUG，具体看这里：

http://oops.kernel.org/oops/warning-at-drivers-gpu-drm-drm_irq-c1326-drm_wait_one_vblank0x191-0x1a0-drm/

vblank BUG会在刚启动的时候导致屏幕无显示，但是在屏幕休眠之后再唤醒则系统正常。(要等屏幕进入休眠，很讨厌)

ubuntu 的 apt source linux 的源码补丁(背光)(4.4.16) 在这里：

https://raw.githubusercontent.com/riverzhou/miix28kernel/miix28/patch/miix28_backlight.4.4.16.patch

内核的配置文件(目前移除了声音)未来可以把声音加回来，还考虑支持摄像头。

https://raw.githubusercontent.com/riverzhou/miix28kernel/miix28/config-4.4.16.localmodconfig

内核的启动参数：

GRUB_CMDLINE_LINUX="intel_idle.max_cstate=1 i915.force_backlight_pmic=1"

intel的ATOM CPU驱动有BUG，睡眠只能支持到C1，如果进入更高的睡眠系统会不稳定。

有问题可以在这个帖子里讨论：

http://forum.ubuntu.org.cn/viewtopic.php?f=48&t=476526


---------------------
update: about wifi

1, firmware


从 git://kernel.ubuntu.com/ubuntu/linux-firmware.git  里下载最新的各种firmware
然后把里面的内容拷贝到  /lib/firmware 目录里

2，配置 /etc/wpa_supplicant.conf


ctrl_interface=/var/run/wpa_supplicant

ctrl_interface_group=root

update_config=1  


network={
        ssid="wifiname"
        #psk="password"
        psk=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
        scan_ssid=1  
        proto=RSN  
        key_mgmt=WPA-PSK  
        pairwise=CCMP  
        auth_alg=OPEN  
}

-------------------
其中 psk 由 wpa_passphrase 生成：
wpa_passphrase wifiname password 


3，设置 rc.local 文件


加入:

/sbin/wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf

/sbin/dhclient wlan0

-------------------
版本 6a90465 及之后的版本 的 wifi 链接效果有大幅度改善

====================================
update for vblank :

vblank的问题非常复杂，深入研究了一下内核的相关代码，
似乎是i915和DRM驱动里的多线程锁的问题，看了很多资料也研究了了代码，还没有清晰的思路。
不过，有个临时性的解决方案。

在rc.local的开始加入两行：

echo 1 > /sys/class/graphics/fb0/blank

echo 0 > /sys/class/graphics/fb0/blank

这样启动之后显示就都正常了。
代码方面我会继续研究。

===================================
update for vblank(final):

GRUB_CMDLINE_LINUX="intel_idle.max_cstate=1 i915.fastboot=1 i915.force_backlight_pmic=1 "

在grub的命令行里加上 i915.fastboot=1 就好了。是kernel在启动时 DSI 驱动的加载时序的问题。

===================================
update for cstate

最新版的kernel已经可以全面使用cstate了, 不再需要intel_idle.max_cstate=1 参数。
测试下来目前都还正常。

GRUB_CMDLINE_LINUX="i915.fastboot=1 i915.enable_psr=1 i915.force_backlight_pmic=1"

===================================
update for bluetooth

最新的kernel已经可以支持蓝牙了，蓝牙鼠标使用的很流畅。

miix28的蓝牙采用uart接口，在ttyS4上。使用前需要先attach:

hciattach -n /dev/ttyS4 bcm43xx

之后

hciconfig  hci0

可以看到蓝牙设备，然后使用bluetoothctl 配对。


另外，可以使用rfkill来打开关闭无线设备：

rfkill list



