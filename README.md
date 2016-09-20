
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


