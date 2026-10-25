#ifndef PROTOCOL_ICD_H
#define PROTOCOL_ICD_H

#include <stdint.h>

#define ICD_START_BYTE 0x7E
#define MAX_PAYLOAD_SIZE 16
#define DEFAULT_ID 0x0

typedef enum {
    CMD_INIT          = 0x00, /* Initialisation */
    CMD_RESET         = 0x01, /* Reinitialisation */
    CMD_ACK           = 0x02, /* Acquittement */
    CMD_MSG           = 0x03, /* Transmission d'un message texte */
    CMD_ERROR         = 0x04, /* Erreur */
    CMD_GET_INFO      = 0x05, /* Demande des informations d'un noeud */
    CMD_INFO_RESPONSE = 0x06  /* Reponse aux informations d'un noeud */
} CommandType_t;

typedef struct __attribute__((packed)) {
    uint8_t  start_byte;
    uint8_t  command_type;
    uint8_t  length;
    uint8_t  reserve;
    uint32_t id_src;
} MessageHeader_t;

typedef struct __attribute__((packed)) {
    MessageHeader_t header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint16_t crc;
} ProtocolMessage_t;

#endif /* PROTOCOL_ICD_H */
