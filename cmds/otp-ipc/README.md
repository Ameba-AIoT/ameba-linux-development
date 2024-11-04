# AmebaD2 OTP IPC Command

## Table of Contents

- [About](#description)
- [Usage](#usage)

## About <a name = "description"></a>

This is Realtek Amebad2 otp IPC command only used for read/write OPTEE HUK, please use efuse command to read/write other address.

## Usage <a name = "usage"></a>

1. read physic test: otp-ipc physic read 0 40
2. write to EFUSE: otp-ipc physic write 5 5 2 2 2 2 2 <cipher>
