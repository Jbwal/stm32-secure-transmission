#ifndef PROTOCOL_ICD_H
#define PROTOCOL_ICD_H

#include <stdint.h>

#define ICD_START_BYTE 0x7E
#define MAX_PAYLOAD_SIZE 16
#define DEFAULT_ID 0X0

// Flags
typedef enum {
    CMD_INIT  = 0x00, // Initialisation
    CMD_RESET = 0x01, // Réinitialisation
    CMD_ACK   = 0x02, // Bonne réception du message
    CMD_MSG   = 0x03, // Transmission d'un message texte
    CMD_ERROR = 0x04  // Erreur 
} CommandType_t;

// Header
typedef struct __attribute__((packed)) {
    uint8_t  start_byte;      // Octet de début de trame (0x7E)
    uint8_t  command_type;   // Type de commande issue de CommandType_t (1 octet)
    uint8_t  length;         // Taille utile du payload (1 octet)
    uint8_t reserve;        // Reserve d'alignement
    uint32_t id_src;         // Identifiant de la source (4 octets)
} MessageHeader_t;


// La trame complète circulant entre les nœuds
typedef struct __attribute__((packed)) {
    MessageHeader_t header;
    uint8_t         payload[MAX_PAYLOAD_SIZE]; // Les données (16 octets max)
    uint16_t        crc;
} ProtocolMessage_t;

#endif // PROTOCOL_ICDH