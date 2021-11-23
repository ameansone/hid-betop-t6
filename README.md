# hid-betop-t6

## 北通宙斯 Linux 驱动模块

北通宙斯在 linux 上可以直接识别为 xbox 手柄，提供标准手柄功能，在大多数情况下都够用了。

但除此之外，北通宙斯还有体感功能，以及四个背键，其中体感功能在 linux 下完全不能用，四个背键只能按照提前设定好的映射模拟成其他按键。

所以我写了这个驱动，它能提供

- 标准 加速度计/陀螺仪（体感） 支持 （有线/接收器模式适用）
- 单独的手柄支持，四个背键映射成独立的按键 （有线模式适用）

单独的手柄支持有点局限，好在体感在有线和接收器模式都能用。

这个驱动几乎完全是从 [dkms-hid-nintendo](https://github.com/nicman23/dkms-hid-nintend) 抄来的，另外，有线模式下安装 [dkms-hid-nintendo](https://github.com/nicman23/dkms-hid-nintend) 能识别成 pro 手柄，同样提供体感功能。

## Betop T6 Linux driver module.

This driver module is primarily for imu support (motion sense).

With [evdevhook](https://github.com/v1993/evdevhoo) it can provide motion source for cemu.

Almost copied from [dkms-hid-nintendo](https://github.com/nicman23/dkms-hid-nintend).

## 依赖 | dependencies

- linux-headers
- dkms (for install)

## 直接使用 | to use

``` shell
git clone https://github.com/ameansone/hid-betop-t6.git
cd hid-betop-t6
make
sudo insmod hid-betop-t6.ko
```

## 安装 | to install

### for arch or arch-based distro

``` shell
git clone https://github.com/ameansone/hid-betop-t6.git
cd hid-betop-t6
makepkg -csi
```

### for others

``` shell
git clone https://github.com/ameansone/hid-betop-t6.git
cd hid-betop-t6
sudo dkms add .
sudo dkms build hid-betop-t6
sudo dkms install hid-betop-t6
```

## cemu 体感 | cemu motion sense

要在 cemu 上开启体感功能需要使用 [evdevhook](https://github.com/v1993/evdevhook)。

evdevhook 需要配置文件，北通宙斯的轴布局和 ns 一样，所以我修改了 ns 的配置放在 evdevhook-config 文件夹下。

### 编译 evdevhook | compile evdevhook

``` shell
git clone https://github.com/v1993/evdevhook.git
cd evdevhook
cmake .
make
```

### 使用 | usage

``` shell
./evdevhook /path/to/hid-betop-t6/evdevhook-config/betop-t6.json
```

关于 evdevhook 的其他内容参阅 [它的主页](https://github.com/v1993/evdevhook)