export PATH

function insmod_ko()
{
    insmod /lib/modules/5.4.63/phy-rtk-usb.ko
    insmod /lib/modules/5.4.63/usb-common.ko
    insmod /lib/modules/5.4.63/udc-core.ko
    insmod /lib/modules/5.4.63/configfs.ko
    #insmod /lib/modules/5.4.63/nls_base.ko
    insmod /lib/modules/5.4.63/libcomposite.ko
    insmod /lib/modules/5.4.63/dwc2.ko
    insmod /lib/modules/5.4.63/usb_f_accessory.ko
    insmod /lib/modules/5.4.63/usb_f_hid.ko
}

function acc_init()
{
    echo "acc_init"

    mkdir /mnt/config/
    mount none /mnt/config/ -t configfs
    cd /mnt/config/
    cd usb_gadget
    mkdir acc
    cd acc
    echo 0x0200 > bcdUSB

    echo 64 > bMaxPacketSize0

    #adb中会根据bDeviceClass，bDeviceSubClass， bDeviceProtocol过滤设备
    echo 0xff > bDeviceClass
    echo 0x42 > bDeviceSubClass
    echo 0x01 > bDeviceProtocol

    echo 0x18d1 > idVendor
    echo 0x4125 > idProduct

    mkdir strings/0x409
    echo "Realtek" > strings/0x409/manufacturer
    echo "ADB Interface" > strings/0x409/product
    echo "12345678911" > strings/0x409/serialnumber

    mkdir configs/c.1
    echo 120 > configs/c.1/MaxPower
    mkdir configs/c.1/strings/0x409
    echo "accessary" > configs/c.1/strings/0x409/configuration

    mkdir functions/accessory.adb
    ln -s functions/accessory.adb configs/c.1/
}

function usb_host_init()
{
    echo "usb host"
    echo 0x2C48 > idVendor
    echo 0x0004 > idProduct
}

function active_device()
{
    echo "Activate device"
    # Activate device
    echo 40080000.usb > UDC
}

function deactive_device()
{
    echo "Deactivate device"
    # Activate device
    echo "" > UDC
}

case $1 in
    "adb")
        insmod_ko
        deactive_device
        acc_init
        active_device
        /bin/adbd &
        ;;
    "usb")
        insmod_ko
        usb_host_init
        active_device
        /bin/adbd &
        ;;
esac
