#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

#define MAXL 250
#define TIMEOUT 5

struct Pachet{
    unsigned char SOH;
    unsigned char LEN;
    unsigned char SEQ;
    char TYPE;
    char DATA[MAXL];
    unsigned short CHECK;
    unsigned char MARK;
};

void pachetSendInit(msg* t, unsigned char currSeq) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'S';
    p.DATA[0] = 150;
    p.DATA[1] = 5;
    p.DATA[2] = 0;
    p.DATA[3] = 0;
    p.DATA[4] = 0x0D;
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void pachetFileHeader(msg* t, unsigned char currSeq, char *nume) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'F';
    strncpy(p.DATA, nume, strlen(nume));
    p.DATA[strlen(nume)] = 0x00;
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void pachetDate(msg* t, unsigned char currSeq, char date[MAXL]) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'D';
    strncpy(p.DATA, date, MAXL);
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void pachetEOF(msg* t) {

}

void pachetEOT(msg* t) {

}

void tryToReceive(unsigned char* currSeq, msg t, char** argv) {
    size_t rep;
    for (rep = 0; rep < 3; rep++)
    {
        msg *y = receive_message_timeout(5000);
        if (y == NULL) {
            perror("receive error");
            send_message(&t);
        } else {
            printf("[%s] Got reply with payload: %s and sequence number: %hhu and type: %c\n", argv[0], y->payload, y->payload[2], y->payload[3]);
            (*currSeq)++;
            break;
        }
    }
}

int main(int argc, char** argv) {
    msg t;
    unsigned char currSeq = 0;

    init(HOST, PORT);


    //sprintf(t.payload, "Hello World of PC");
    //t.len = strlen(t.payload);
    //send_message(&t);

    size_t i;
    for (i = 1; i < argc; i++)
    {
        //"S"
        pachetSendInit(&t, currSeq);
        send_message(&t);
        tryToReceive(&currSeq, t, argv);

        //"F"
        pachetFileHeader(&t, currSeq, argv[i]);
        send_message(&t);
        tryToReceive(&currSeq, t, argv);


        //"D"
        FILE* file = NULL;
        char buffer[MAXL];
        size_t bytesRead = 0;

        file = fopen(argv[i], "rb");

        if (file != NULL)
        {
            while ((bytesRead = fread(buffer, 1, sizeof(buffer), file) > 0))
            {
                int size = strlen(buffer);
                buffer[size-1] = 0x00;
                pachetDate(&t, currSeq, buffer);
                send_message(&t);
                tryToReceive(&currSeq, t, argv);
            }
        }
        else 
        {
            perror("File not found");
            return -1;
        }

        fclose(file);
    }
    return 0;
}