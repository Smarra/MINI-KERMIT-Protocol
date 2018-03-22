#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

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

void pachetACK(msg* t, unsigned char currSeq) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'Y';
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

void pachetNAK(msg* t, unsigned char currSeq) {
    struct Pachet p;
    p.SOH = 1;
    p.LEN = MAXL + 7 - 2;
    p.SEQ = currSeq;
    p.TYPE = 'N';
    p.CHECK = crc16_ccitt(&p, p.LEN - 1);
    p.MARK = 0x0D;

    memcpy(t->payload, &p, p.LEN + 2);
    t->len = p.LEN + 2;
}

int main(int argc, char** argv) {
    msg r, t;
    unsigned char currSeq = 0;
    unsigned char prevSeq = 0;
    char numeFisier[40];
    unsigned char endOfTransimission = 1;

    FILE* f = NULL;

    init(HOST, PORT);

    while(endOfTransimission)
    {
        if (recv_message(&r) < 0 || 
           (((unsigned char) r.payload[r.len-2]) << 8) + ((unsigned char) r.payload[r.len-3]) != crc16_ccitt(r.payload, r.len-3)) 
        {
            perror("Receive message");
            pachetNAK(&t, currSeq);
            send_message(&t);
        }
        else
        {
            printf("[%s] Got msg with sequence number: %hhu and type: %c\n", argv[0], r.payload[2], r.payload[3]);

            switch(r.payload[3]) {
                case 'F':   
                    strcpy(numeFisier, "recv_");
                    strcat(numeFisier, &r.payload[4]);
                    f = fopen(numeFisier, "w");
                    fclose(f);
                    f = fopen(numeFisier, "a");
                    break;

                case 'D':
                    //printf("Continut: %s\n", &r.payload[5]);
                    //fprintf(f, "%s", &r.payload[5]);
                    fwrite(&r.payload[5], sizeof(char), (unsigned char)r.payload[4], f);
                    break;

                case 'Z':
                    fclose(f);
                    break;

                case 'B':
                    endOfTransimission = 0;
                    break;
            }

            prevSeq = currSeq;
            currSeq = r.payload[2];

            pachetACK(&t, currSeq);
            send_message(&t);
        }
    }
	
	return 0;
}
