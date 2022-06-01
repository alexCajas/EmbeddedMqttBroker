#ifndef CONTROLPACKETTYPE_H
#define CONTROLPACKETTYPE_H
/**
 * @brief Types of mqtt packets
 * 
 */
#define CONNECT     1       // Client request to connect to Server
#define CONNECTACK  2       // Connect Acknowledgment
#define PUBLISH     3       // Publish message
#define PUBACK      4       // Publish Acknowledgment
#define PUBREC      5       // Publish Received (assured delivery part 1)
#define PUBREL      6       // Publish Release (assured delivery part 2)
#define PUBCOMP     7       // Publish Complete (assured delivery part 3)
#define SUBSCRIBE   8       // Client Subscribe request
#define SUBACK      9       // Subscribe Acknowledgment
#define UNSUBSCRIBE 10      // Client Unsubscribe request
#define UNSUBACK    11      // Unsubscribe Acknowledgment
#define PINGREQ     12      // PING Request
#define PINGRESP    13      // PING Response
#define DISCONNECT  14      // Client is Disconnecting
#define Reserved    15      // Reserved

/**
 * @brief Reserved bits int first byte fo fixedHeader.
 * 
 */
#define RESERVERTO0 0
#define RESERVERTO2 2

/**
 * @brief Code to response a Connect packet
 * 
 */
#define CONNECTACCEPTED         0
#define REFUSEDPROTOCOLVERSION  1 // Server not support protocol version
#define REFUSEIDENTIFIER        2 // server not allowed client id
#define REFUSEDSERVER           3 // server is not unavilable 
#define REFUSEDNAMEPASS         4 // malformed name or pass
#define REFUSEDAUTHORIZACION    5 // client is not authorize to connect

/**
 * @brief Sesion present
 * 
 */
#define NOTSESIONPRESENT    0
#define SESIONPRESENT       1

#endif // CONTROLPACKETTYPES_H