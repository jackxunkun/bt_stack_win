#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <SetupAPI.h>
#include <Cfgmgr32.h>
#include <tchar.h>
#include <strsafe.h>
#include <winusb.h>


BOOL GetDevicePath(OUT PTCHAR pDeviceName, LPGUID pGuid,IN size_t BufLen)
{
    BOOL bResult = FALSE;
    HDEVINFO deviceInfo;
    SP_DEVICE_INTERFACE_DATA interfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
    ULONG length;
    ULONG  requiredLength = 0;
    HRESULT hr;
    DWORD memberIndex = 0;
    //通过GUID获取设备信息集句柄
    deviceInfo = SetupDiGetClassDevsW(pGuid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (deviceInfo == INVALID_HANDLE_VALUE)
    {
        printf("SetupDiGetClassDevsW fail \n");
        return bResult;
    }
    //获取设备接口信息
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA );
    while(SetupDiEnumDeviceInterfaces(deviceInfo,NULL,pGuid,memberIndex,&interfaceData)) {
        memberIndex++;
        //获取设备接口的详细数据
        SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData, NULL, 0, &requiredLength, NULL);
        detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LMEM_FIXED, requiredLength);
        if (detailData == NULL) {
            SetupDiDestroyDeviceInfoList(deviceInfo);
            return FALSE;
        }
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        length = requiredLength;
        bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo, &interfaceData, detailData, length, &requiredLength,
                                                  NULL);
        if (FALSE == bResult) {
            LocalFree(detailData);
            return FALSE;
        }
        //获取设备路径
        printf("%s\n",detailData->DevicePath);
        hr = StringCchCopy(pDeviceName, BufLen, detailData->DevicePath);
        if (FAILED(hr)) {
            SetupDiDestroyDeviceInfoList(deviceInfo);
        }
        LocalFree(detailData);
    }
    return bResult;
}
HANDLE OpenDevice()
{
    HANDLE hDev = NULL;
    // 定义设备接口的 GUID（例如：USB 设备接口的 GUID）
    GUID guid;
    // 设置为 USB 设备接口的 GUID
    // {A5DCBF10-6530-11D2-901F-00C04FB951ED}
    CLSIDFromString(L"{A5DCBF10-6530-11D2-901F-00C04FB951ED}", &guid);
    TCHAR strDevicePath[512] = { 0 };
    BOOL retval;
    //通过GUID获取设备路径
    retval = GetDevicePath(strDevicePath, &guid, sizeof(strDevicePath));
    if (retval == FALSE)
    {
        return hDev;
    }
    //通过设备路径获取文件句柄
    hDev = CreateFile(strDevicePath,GENERIC_WRITE|GENERIC_READ,FILE_SHARE_WRITE|FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,NULL);
    return hDev;
}
int main_1()
{
    BOOL bResult;
    //获取设备句柄
    HANDLE  file_hand = OpenDevice();
    //获取win usb 句柄
    static WINUSB_INTERFACE_HANDLE usbHandle;
    bResult = WinUsb_Initialize(file_hand,&usbHandle);
    if(!bResult)
    {
        printf("WinUsb Initialize faile\n");
        return FALSE;
    }
    //获取usb设备的速度
    ULONG length = sizeof(UCHAR);
    UCHAR speed;
    bResult = WinUsb_QueryDeviceInformation(usbHandle,DEVICE_SPEED,&length,&speed);
    if(bResult)
    {
        printf("0x%x\n",speed);
    }
    //获取接口描述
    static int event_in_addr;
    static int acl_in_addr;
    static int acl_out_addr;
    USB_INTERFACE_DESCRIPTOR usb_interface_descriptor;
    bResult = WinUsb_QueryInterfaceSettings(usbHandle,0,&usb_interface_descriptor);
    if (bResult)
    {
        // Detect USB Dongle based Class, Subclass, and Protocol
        // The class code (bDeviceClass) is 0xE0 – Wireless Controller.
        // The SubClass code (bDeviceSubClass) is 0x01 – RF Controller.
        // The Protocol code (bDeviceProtocol) is 0x01 – Bluetooth programming.
        printf("class:0x%x\n",usb_interface_descriptor.bInterfaceClass);
        printf("subclass:0x%x\n",usb_interface_descriptor.bInterfaceSubClass);
        printf("Protocol:0x%x\n",usb_interface_descriptor.bInterfaceProtocol);
        //bNumEndpoints
        printf("bNumEndpoints:%d\n",usb_interface_descriptor.bNumEndpoints);
        //获取每个端点的信息
        for (int i=0;i<usb_interface_descriptor.bNumEndpoints;i++)
        {
            WINUSB_PIPE_INFORMATION pipe;
            bResult = WinUsb_QueryPipe(usbHandle,0,(UCHAR) i,&pipe);
            if(bResult)
            {
                printf("Interface #0, Alt #0, Pipe idx #%u: type %u, id 0x%02x, max packet size %u,\n",
                         i, pipe.PipeType, pipe.PipeId, pipe.MaximumPacketSize);
                switch (pipe.PipeType)
                {
                    case USB_ENDPOINT_TYPE_INTERRUPT:
                        if (event_in_addr) continue;
                        event_in_addr = pipe.PipeId;
                        printf("-> using 0x%2.2X for HCI Events\n", event_in_addr);
                        break;
                    case USB_ENDPOINT_TYPE_BULK:
                        if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId)) {
                            if (acl_in_addr) continue;
                            acl_in_addr = pipe.PipeId;
                            printf("-> using 0x%2.2X for ACL Data In\n", acl_in_addr);
                        } else {
                            if (acl_out_addr) continue;
                            acl_out_addr = pipe.PipeId;
                            printf("-> using 0x%2.2X for ACL Data Out\n", acl_out_addr);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        printf("event_in_addr:0x%x\n",event_in_addr);
        printf("acl_in_addr:0x%x\n",acl_in_addr);
        printf("acl_out_addr:0x%x\n",acl_out_addr);
    }
    //异步写入
    OVERLAPPED overlapped = {0};
    UCHAR buffer[] = "Hello, USB!";
    ULONG bufferLength = sizeof(buffer);
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (overlapped.hEvent == NULL) {
        printf("Error creating event: %lu\n", GetLastError());
        return FALSE;
    }

    ULONG bytesWritten = 0;
    BOOL result = WinUsb_WritePipe(usbHandle, acl_out_addr, buffer, bufferLength, &bytesWritten, &overlapped);

    if (!result) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            printf("Error writing to USB device: %lu\n", error);
            CloseHandle(overlapped.hEvent);
            return FALSE;
        }

        // 等待 I/O 操作完成,下面的API的最后一个参数决定是否阻塞
        result = GetOverlappedResult((HANDLE)usbHandle, &overlapped, &bytesWritten, TRUE);
        if (!result) {
            printf("Error getting overlapped result: %lu\n", GetLastError());
        } else {
            printf("Successfully wrote %lu bytes to USB device.\n", bytesWritten);
        }
    }
    CloseHandle(overlapped.hEvent);

    WinUsb_Free(usbHandle);
    CloseHandle(file_hand);
}

int main()
{

}