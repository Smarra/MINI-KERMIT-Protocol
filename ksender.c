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
#define MOD 64

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

void pachetDate(msg* t, unsigned char currSeq, char date[MAXL], int length) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'D';
    memcpy(p.DATA, date, length);
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void pachetEOF(msg* t, unsigned char currSeq) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'Z';
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void pachetEOT(msg* t, unsigned char currSeq) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'B';
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void tryToReceive(unsigned char* prevSeq, unsigned char* currSeq, msg t, char** argv) {
    size_t rep, timeoutCounter = 0;
    while(1)
    {
        msg *y = receive_message_timeout(5000);
        if (y == NULL || (char)y->payload[3] == 'N'  || 
           (*currSeq != (unsigned char)y->payload[2] || 
           (*currSeq == 0 && (unsigned char)y->payload[2] == 63))) 
        {
            if (y == NULL)
            {
                timeoutCounter++;
                if (timeoutCounter == 3)
                    exit(1);
            }
            perror("receive error");
            send_message(&t);
        } else {
            printf("[%s] Got reply with sequence number: %hhu and type: %c\n", argv[0], y->payload[2], y->payload[3]);
            *prevSeq = *currSeq;
            (*currSeq) = ((*currSeq) + 1) % MOD;
            break;
        }
    }
}

int main(int argc, char** argv) {
    msg t;
    unsigned char currSeq = 0;
    unsigned char prevSeq = 0;

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
        tryToReceive(&prevSeq, &currSeq, t, argv);

        //"F"
        pachetFileHeader(&t, currSeq, argv[i]);
        send_message(&t);
        tryToReceive(&prevSeq, &currSeq, t, argv);


        //"D"
        FILE* file = NULL;
        char buffer[MAXL];
        size_t bytesRead = 0;

        file = fopen(argv[i], "rb");

        if (file != NULL)
        {
            while (1)
            {
                bytesRead = fread(buffer + 1, 1, sizeof(buffer) - 2, file);
                if( bytesRead <= 0 )
                    break;
                buffer[0] = bytesRead;
                buffer[bytesRead+1] = 0x00; 
                pachetDate(&t, currSeq, buffer, bytesRead + 1);
                send_message(&t);
                tryToReceive(&prevSeq, &currSeq, t, argv);
            }

            //"Z"
            pachetEOF(&t, currSeq);
            send_message(&t);
            tryToReceive(&prevSeq, &currSeq, t, argv);
        }
        else 
        {
            perror("File not found");
            return -1;
        }

        fclose(file);
    }
    //"B"
    pachetEOT(&t, currSeq);
    send_message(&t);
    tryToReceive(&prevSeq, &currSeq, t, argv);

    return 0;
}