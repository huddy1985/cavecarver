// deviceqemu.c : implementation related to dummy qemu.
// https://wiki.qemu.org/Documentation/QMP
//
// (c) Ulf Frisk, 2018
// Author: Ulf Frisk, pcileech@frizk.net
//

extern "C" {
#include "deviceqemu.h"
#include "util.h"
}
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOCKET int
#define INVALID_SOCKET	-1
#define SOCKET_ERROR	-1
#define closesocket(_s_) close((_s_))

#undef min
#undef max
#include "nlohmann/json.hpp"
#include <string>
#include <iostream>
#include <sstream>
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

typedef struct tdDEVICE_CONTEXT_QEMU {
    SOCKET Sock;
    bool verbose;
    FILE *pFile;
    QWORD cbFile;
    LPSTR szFileName;
} DEVICE_CONTEXT_QEMU, *PDEVICE_CONTEXT_QEMU;


using json = nlohmann::json;

/* aaaack but it's fast and const should make it shared text page. */
static const unsigned char pr2six[256] =
{
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

int Base64decode_len(const char *bufcoded)
{
    int nbytesdecoded;
    register const unsigned char *bufin;
    register int nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);

    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    return nbytesdecoded + 1;
}

int Base64decode(char *bufplain, const char *bufcoded)
{
    int nbytesdecoded;
    register const unsigned char *bufin;
    register unsigned char *bufout;
    register int nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);
    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    bufout = (unsigned char *) bufplain;
    bufin = (const unsigned char *) bufcoded;

    while (nprbytes > 4) {
    *(bufout++) =
        (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    *(bufout++) =
        (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    *(bufout++) =
        (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    bufin += 4;
    nprbytes -= 4;
    }

    /* Note: (nprbytes == 1) would be an error, so just ingore that case */
    if (nprbytes > 1) {
    *(bufout++) =
        (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    }
    if (nprbytes > 2) {
    *(bufout++) =
        (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    }
    if (nprbytes > 3) {
    *(bufout++) =
        (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    }

    *(bufout++) = '\0';
    nbytesdecoded -= (4 - nprbytes) & 3;
    return nbytesdecoded;
}

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int Base64encode_len(int len)
{
    return ((len + 2) / 3 * 4) + 1;
}

std::string dbg_str(std::string a) {
    std::stringstream b;
    for (auto i : a) {
	if (isprint(i)) {
	    b << i;
	} else {
	    b << ":";
	}
    }
    return b.str();
}

int Base64encode(char *encoded, const char *string, int len)
{
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
    *p++ = basis_64[(string[i] >> 2) & 0x3F];
    *p++ = basis_64[((string[i] & 0x3) << 4) |
                    ((int) (string[i + 1] & 0xF0) >> 4)];
    *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                    ((int) (string[i + 2] & 0xC0) >> 6)];
    *p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
    *p++ = basis_64[(string[i] >> 2) & 0x3F];
    if (i == (len - 1)) {
        *p++ = basis_64[((string[i] & 0x3) << 4)];
        *p++ = '=';
    }
    else {
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int) (string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
    }
    *p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}

SOCKET DeviceQemu_TCP_Connect(_In_ DWORD Addr, _In_ WORD Port)
{
	SOCKET Sock = 0;
	struct sockaddr_in sAddr;
	sAddr.sin_family = AF_INET;
	sAddr.sin_port = htons(Port);
	sAddr.sin_addr.s_addr = Addr;
	if ((Sock = socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET) {
		if (connect(Sock, (struct sockaddr *)&sAddr, sizeof(sAddr)) != SOCKET_ERROR) { return Sock; }
		fprintf(stderr, "ERROR: connect() fails\n");
		closesocket(Sock);
	}
	else {
		fprintf(stderr, "ERROR: socket() fails\n");
	}
	return 0;
}

int qemu_readline(PDEVICE_CONTEXT_QEMU ctx, std::string &str) {
    std::stringstream b; char c[2];
    while (1) {
	int readlen = recv(ctx->Sock, c, 1, 0);
	if (readlen <= 0)
	    break;
	if (c[0] == '\n')
	    break;
	b << c[0];
    }
    str = b.str();
    return 0;
}

int qemu_result_or_event(PDEVICE_CONTEXT_QEMU ctx, json &result, std::vector<json> &events, bool any=false) {
    std::string str;
    do {
	json j;
	str = "";
	qemu_readline(ctx, str);
	try {
	    j = json::parse(str);
	    if(ctx->verbose) {
		std::cout << "[>] " << j.dump() << std::endl;
	    }
	    if (any) {
		result = j;
		return 0;
	    }
	    if (j.find("event") != j.end()) {
		events.push_back(j);
	    } else {
		result = j;
		break;
	    }
	} catch(nlohmann::detail::exception &e) {
	    printf("Error parsing '%s'\n", (char*)dbg_str(str).c_str());
	}
    } while(1);
    return 0;
}

int qemu_send_cmd(PDEVICE_CONTEXT_QEMU ctx, const char *cmd, json &result, std::vector<json> &events) {
    int cmdlen = strlen(cmd); int r;
    if(ctx->verbose) {
	std::cout << "[<] " << cmd << std::endl;
    }
    int sent = send(ctx->Sock, cmd, cmdlen, 0);
    if (sent != cmdlen) {
	printf("ERROR: send() fails\n");
	return 1;
    }
    r = qemu_result_or_event(ctx, result, events);
    return r;
}

int qemu_read(PDEVICE_CONTEXT_QEMU ctx, QWORD addr, DWORD len, PBYTE n,  DWORD &rl) {
    char b[256];
    json result; int r;
    std::vector<json> events;
    sprintf(b, "{'execute':'rdpy','arguments':{'addr':%lld,'size':%d}}", addr, len);
    if (r = qemu_send_cmd(ctx, b, result, events)) {
	return r;
    }

    if (result.find("return") == result.end()) {
	return 1;
    }

    std::string d = result["return"];
    int decodebuflen = Base64decode_len(d.c_str());

    char *decodeddata = (char *)LocalAlloc(LMEM_ZEROINIT, decodebuflen);
    int decodedlen = Base64decode(decodeddata, d.c_str());
    if (len != decodedlen) {
	printf("Error: readlen is %d unequal reqested len %d", decodedlen, len);
    }
    int copylen = min(decodedlen, len);
    memcpy(n, decodeddata, copylen);

    rl = copylen;
    LocalFree(decodeddata);
    return 0;
}

int qemu_negotiate(PDEVICE_CONTEXT_QEMU ctx) {
    json result;
    std::vector<json> events;
    qemu_result_or_event(ctx, result, events, true);
    if (result.find("QMP") != result.end()) {
	return qemu_send_cmd(ctx, "{'execute':'qmp_capabilities'}", result, events);
    }
    return 1;
}


/*******************************************************************************/


VOID DeviceQemu_ReadScatterDMA(_Inout_ PPCILEECH_CONTEXT ctx, _Inout_ PPDMA_IO_SCATTER_HEADER ppDMAs, _In_ DWORD cpDMAs, _Out_opt_ PDWORD pchDMAsRead)
{
    PDEVICE_CONTEXT_QEMU ctxFile = (PDEVICE_CONTEXT_QEMU)ctx->hDevice;
    DWORD i, cbToRead, c = 0;
    PDMA_IO_SCATTER_HEADER pDMA;
    printf("[r] \n");
    for(i = 0; i < cpDMAs; i++) {
	int result;
	pDMA = ppDMAs[i];

	result = qemu_read(ctxFile, pDMA->qwA, pDMA->cbMax, pDMA->pb, pDMA->cb);

	printf("[r] [%016llx:%08x] => %08x\n", pDMA->qwA, pDMA->cbMax, pDMA->cb);

	if(ctx->cfg->fVerboseExtraTlp) {
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

    printf("[p] [%016llx:%08x]\n", qwAddr, cPages);

    for(i = 0; i < cPages; i++) {
	char b[1<<12]; DWORD rl;
	if (qemu_read(ctxFile, (qwAddr + (i << 12)), 4, (PBYTE)b, rl)) {
	    pbResultMap[i] = 0;
	} else {
	    pbResultMap[i] = 1;
	}
    }
}

VOID DeviceQemu_Close(_Inout_ PPCILEECH_CONTEXT ctx)
{
    PDEVICE_CONTEXT_QEMU ctxFile = (PDEVICE_CONTEXT_QEMU)ctx->hDevice;
    if(!ctxFile) { return; }

    if (ctxFile->Sock) { closesocket(ctxFile->Sock); }
    LocalFree(ctxFile);
    ctx->hDevice = 0;
}

BOOL DeviceQemu_Open(_Inout_ PPCILEECH_CONTEXT ctx)
{
    PDEVICE_CONTEXT_QEMU ctxFile;
    ctxFile = (PDEVICE_CONTEXT_QEMU)LocalAlloc(LMEM_ZEROINIT, sizeof(DEVICE_CONTEXT_QEMU));
    if(!ctxFile) { return FALSE; }

    ctxFile->Sock = DeviceQemu_TCP_Connect(ctx->cfg->TcpAddr, ctx->cfg->TcpPort);
    if (!ctxFile->Sock) { goto fail; }

    ctx->hDevice = (HANDLE)ctxFile;
    ctxFile->verbose = ctx->cfg->fVerbose;

    ctx->cfg->dev.tp = PCILEECH_DEVICE_QEMU;
    ctx->cfg->dev.qwMaxSizeDmaIo = 0x00100000;          // 1MB
    ctx->cfg->dev.qwAddrMaxNative = 0x0000ffffffffffff;
    ctx->cfg->dev.fPartialPageReadSupported = TRUE;
    ctx->cfg->dev.pfnClose = DeviceQemu_Close;
    ctx->cfg->dev.pfnProbeDMA = DeviceQemu_ProbeDMA;
    ctx->cfg->dev.pfnReadScatterDMA = DeviceQemu_ReadScatterDMA;
    if(ctx->cfg->fVerbose) {
        printf("DEVICE: Successfully opened qemu\n");
    }

    if (qemu_negotiate(ctxFile))
	goto fail;

    return TRUE;
fail:
    LocalFree(ctxFile);
    printf("DEVICE: ERROR: Failed opening qemu\n");
    return FALSE;
}
