// STANDARD HEADER:
// Octal strings have to be null/space terminated.
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char modtime[12]; 
    char checksum[8]; // Sum of all of the bytes (unsighed chars) of the header written into 6-digit octal and a \0 with a space.
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} tar_header;

// PAX:
// Pax supports some keywords: path and size.
// When a file triggers a PAX requirement it follows this:
// Line Length + " " + KEYWORD + "=" + VALUE + "\n"
// Where:
//  Line length is the length of the line including the length of the characters defining the length.