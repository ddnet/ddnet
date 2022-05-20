#include "masterserver.h"

const unsigned char SERVERBROWSE_GETINFO[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'g', 'i', 'e', '3'};
const unsigned char SERVERBROWSE_INFO[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'i', 'n', 'f', '3'};

const unsigned char SERVERBROWSE_GETINFO_64_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'f', 's', 't', 'd'};
const unsigned char SERVERBROWSE_INFO_64_LEGACY[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'd', 't', 's', 'f'};

const unsigned char SERVERBROWSE_INFO_EXTENDED[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'i', 'e', 'x', 't'};
const unsigned char SERVERBROWSE_INFO_EXTENDED_MORE[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'i', 'e', 'x', '+'};

const unsigned char SERVERBROWSE_CHALLENGE[SERVERBROWSE_SIZE] = {255, 255, 255, 255, 'c', 'h', 'a', 'l'};
