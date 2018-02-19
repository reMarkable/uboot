#!/bin/bash

scp u-boot.imx root@10.11.99.1:.
ssh root@10.11.99.1 'echo 0 > /sys/block/mmcblk1boot0/force_ro'
ssh root@10.11.99.1 'dd if=u-boot.imx of=/dev/mmcblk1boot0 bs=512 seek=2'
ssh root@10.11.99.1 'echo 1 > /sys/block/mmcblk1boot0/force_ro'
