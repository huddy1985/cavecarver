// deviceqemu.c : implementation related to dummy qemu.
// https://wiki.qemu.org/Documentation/QMP
//
// (c) Ulf Frisk, 2018
// Author: Ulf Frisk, pcileech@frizk.net
//
#include "devicefile.h"
#include "util.h"

typedef struct tdDEVICE_CONTEXT_QEMU {
    FILE *pFile;
    QWORD cbFile;
    LPSTR szFileName;
} DEVICE_CONTEXT_QEMU, *PDEVICE_CONTEXT_QEMU;

VOID DeviceQemu_ReadScatterDMA(_Inout_ PPCILEECH_CONTEXT ctx, _Inout_ PPDMA_IO_SCATTER_HEADER ppDMAs, _In_ DWORD cpDMAs, _Out_opt_ PDWORD pchDMAsRead)
{
    PDEVICE_CONTEXT_QEMU ctxFile = (PDEVICE_CONTEXT_QEMU)ctx->hDevice;
    DWORD i, cbToRead, c = 0;
    PDMA_IO_SCATTER_HEADER pDMA;
    for(i = 0; i < cpDMAs; i++) {
        pDMA = ppDMAs[i];
        if(pDMA->qwA >= ctxFile->cbFile) { continue; }
        cbToRead = (DWORD)min(pDMA->cb, ctxFile->cbFile - pDMA->qwA);
        if(pDMA->qwA != _ftelli64(ctxFile->pFile)) {
            if(_fseeki64(ctxFile->pFile, pDMA->qwA, SEEK_SET)) { continue; }
        }
        pDMA->cb = (DWORD)fread(pDMA->pb, 1, pDMA->cbMax, ctxFile->pFile);
        if(ctx->cfg->fVerboseExtraTlp) {
            printf("devicefile.c!DeviceQemu_ReadScatterDMA: READ: file=%s offset=%016llx req_len=%08x rsp_len=%08x\n", ctxFile->szFileName, pDMA->qwA, pDMA->cbMax, pDMA->cb);
            Util_PrintHexAscii(pDMA->pb, pDMA->cb, 0);
        }
        c += (ppDMAs[i]->cb >= ppDMAs[i]->cbMax) ? 1 : 0;
    }
    if(pchDMAsRead) {
        *pchDMAsRead = c;
    }
}

VOID DeviceQemu_ProbeDMA(_Inout_ PPCILEECH_CONTEXT ctx, _In_ QWORD qwAddr, _In_ DWORD cPages, _Inout_ __bcount(cPages) PBYTE pbResultMap)
{
    PDEVICE_CONTEXT_QEMU ctxFile = (PDEVICE_CONTEXT_QEMU)ctx->hDevice;
    QWORD i;
    for(i = 0; i < cPages; i++) {
        pbResultMap[i] = ((qwAddr + (i << 12)) < ctxFile->cbFile) ? 1 : 0;
    }
}

VOID DeviceQemu_Close(_Inout_ PPCILEECH_CONTEXT ctx)
{
    PDEVICE_CONTEXT_QEMU ctxFile = (PDEVICE_CONTEXT_QEMU)ctx->hDevice;
    if(!ctxFile) { return; }
    fclose(ctxFile->pFile);
    LocalFree(ctxFile);
    ctx->hDevice = 0;
}

BOOL DeviceQemu_Open(_Inout_ PPCILEECH_CONTEXT ctx)
{
    PDEVICE_CONTEXT_QEMU ctxFile;
    ctxFile = (PDEVICE_CONTEXT_QEMU)LocalAlloc(LMEM_ZEROINIT, sizeof(DEVICE_CONTEXT_QEMU));
    if(!ctxFile) { return FALSE; }
    // open backing file
    if(fopen_s(&ctxFile->pFile, ctx->cfg->dev.szFileNameOptTpFile, "rb") || !ctxFile->pFile) { goto fail; }
    if(_fseeki64(ctxFile->pFile, 0, SEEK_END)) { goto fail; }       // seek to end of file
    ctxFile->cbFile = _ftelli64(ctxFile->pFile);                    // get current file pointer
    if(ctxFile->cbFile < 0x1000) { goto fail; }
    ctxFile->szFileName = ctx->cfg->dev.szFileNameOptTpFile;
    ctx->hDevice = (HANDLE)ctxFile;
    // set callback functions and fix up config
    ctx->cfg->dev.tp = PCILEECH_DEVICE_QEMU;
    ctx->cfg->dev.qwMaxSizeDmaIo = 0x00100000;          // 1MB
    ctx->cfg->dev.qwAddrMaxNative = ctxFile->cbFile;
    ctx->cfg->dev.fPartialPageReadSupported = TRUE;
    ctx->cfg->dev.pfnClose = DeviceQemu_Close;
    ctx->cfg->dev.pfnProbeDMA = DeviceQemu_ProbeDMA;
    ctx->cfg->dev.pfnReadScatterDMA = DeviceQemu_ReadScatterDMA;
    if(ctx->cfg->fVerbose) {
        printf("DEVICE: Successfully opened file: '%s'.\n", ctx->cfg->dev.szFileNameOptTpFile);
    }
    return TRUE;
fail:
    if(ctxFile->pFile) { fclose(ctxFile->pFile); }
    LocalFree(ctxFile);
    printf("DEVICE: ERROR: Failed opening file: '%s'.\n", ctx->cfg->dev.szFileNameOptTpFile);
    return FALSE;
}
