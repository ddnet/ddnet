#include "mastersrv.h"

const int MASTERSERVER_PORT = 8300;

const unsigned char SERVERBROWSE_HEARTBEAT[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'b', 'e', 'a', '2'};

const unsigned char SERVERBROWSE_GETLIST[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'r', 'e', 'q', '2'};
const unsigned char SERVERBROWSE_LIST[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'l', 'i', 's', '2'};

const unsigned char SERVERBROWSE_GETCOUNT[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'c', 'o', 'u', '2'};
const unsigned char SERVERBROWSE_COUNT[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 's', 'i', 'z', '2'};

const unsigned char SERVERBROWSE_GETINFO[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'g', 'i', 'e', '3'};
const unsigned char SERVERBROWSE_INFO[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'i', 'n', 'f', '3'};

const unsigned char SERVERBROWSE_GETINFO_64_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'f', 's', 't', 'd'};
const unsigned char SERVERBROWSE_INFO_64_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'd', 't', 's', 'f'};

const unsigned char SERVERBROWSE_INFO_EXTENDED[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'i', 'e', 'x', 't'};
const unsigned char SERVERBROWSE_INFO_EXTENDED_MORE[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'i', 'e', 'x', '+'};

const unsigned char SERVERBROWSE_FWCHECK[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'f', 'w', '?', '?'};
const unsigned char SERVERBROWSE_FWRESPONSE[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'f', 'w', '!', '!'};
const unsigned char SERVERBROWSE_FWOK[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'f', 'w', 'o', 'k'};
const unsigned char SERVERBROWSE_FWERROR[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'f', 'w', 'e', 'r'};

const unsigned char SERVERBROWSE_HEARTBEAT_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'b', 'e', 'a', 't'};

const unsigned char SERVERBROWSE_GETLIST_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'r', 'e', 'q', 't'};
const unsigned char SERVERBROWSE_LIST_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'l', 'i', 's', 't'};

const unsigned char SERVERBROWSE_GETCOUNT_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'c', 'o', 'u', 'n'};
const unsigned char SERVERBROWSE_COUNT_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 's', 'i', 'z', 'e'};
