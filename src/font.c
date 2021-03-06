// 6x3 bitmap font, start from ASCII space
const char font[][3] = {
	{ 0x00, 0x00, 0x00 }, /* ' ' */
	{ 0x00, 0x17, 0x00 }, /* '!' */
	{ 0x03, 0x00, 0x03 }, /* '"' */
	{ 0x1F, 0x0A, 0x1F }, /* '#' */
	{ 0x0A, 0x1F, 0x05 }, /* '$' */
	{ 0x19, 0x04, 0x13 }, /* '%' */
	{ 0x0F, 0x17, 0x1C }, /* '&' */
	{ 0x00, 0x03, 0x00 }, /* ''' */
	{ 0x00, 0x0E, 0x11 }, /* '(' */
	{ 0x11, 0x0E, 0x00 }, /* ')' */
	{ 0x15, 0x0E, 0x15 }, /* '*' */
	{ 0x04, 0x0E, 0x04 }, /* '+' */
	{ 0x20, 0x10, 0x00 }, /* ',' */
	{ 0x04, 0x04, 0x04 }, /* '-' */
	{ 0x00, 0x10, 0x00 }, /* '.' */
	{ 0x18, 0x04, 0x03 }, /* '/' */
	{ 0x1F, 0x11, 0x1F }, /* '0' */
	{ 0x12, 0x1F, 0x10 }, /* '1' */
	{ 0x11, 0x19, 0x16 }, /* '2' */
	{ 0x15, 0x15, 0x0A }, /* '3' */
	{ 0x0C, 0x0A, 0x1F }, /* '4' */
	{ 0x17, 0x15, 0x0D }, /* '5' */
	{ 0x1E, 0x15, 0x1D }, /* '6' */
	{ 0x01, 0x19, 0x07 }, /* '7' */
	{ 0x1F, 0x15, 0x1F }, /* '8' */
	{ 0x17, 0x15, 0x0F }, /* '9' */
	{ 0x00, 0x0A, 0x00 }, /* ':' */
	{ 0x10, 0x0A, 0x00 }, /* ';' */
	{ 0x04, 0x0A, 0x11 }, /* '<' */
	{ 0x0A, 0x0A, 0x0A }, /* '=' */
	{ 0x11, 0x0A, 0x04 }, /* '>' */
	{ 0x01, 0x15, 0x02 }, /* '?' */
	{ 0x0E, 0x11, 0x17 }, /* '@' */
	{ 0x1E, 0x05, 0x1E }, /* 'A' */
	{ 0x1F, 0x15, 0x0A }, /* 'B' */
	{ 0x0E, 0x11, 0x11 }, /* 'C' */
	{ 0x1F, 0x11, 0x0E }, /* 'D' */
	{ 0x1F, 0x15, 0x15 }, /* 'E' */
	{ 0x1F, 0x05, 0x05 }, /* 'F' */
	{ 0x0E, 0x11, 0x1D }, /* 'G' */
	{ 0x1F, 0x04, 0x1F }, /* 'H' */
	{ 0x11, 0x1F, 0x11 }, /* 'I' */
	{ 0x11, 0x1F, 0x01 }, /* 'J' */
	{ 0x1F, 0x04, 0x1B }, /* 'K' */
	{ 0x1F, 0x10, 0x10 }, /* 'L' */
	{ 0x1F, 0x06, 0x1F }, /* 'M' */
	{ 0x1F, 0x01, 0x1E }, /* 'N' */
	{ 0x1E, 0x11, 0x0F }, /* 'O' */
	{ 0x1F, 0x05, 0x02 }, /* 'P' */
	{ 0x1E, 0x31, 0x2F }, /* 'Q' */
	{ 0x1F, 0x05, 0x1A }, /* 'R' */
	{ 0x12, 0x15, 0x09 }, /* 'S' */
	{ 0x01, 0x1F, 0x01 }, /* 'T' */
	{ 0x0F, 0x10, 0x1F }, /* 'U' */
	{ 0x07, 0x18, 0x07 }, /* 'V' */
	{ 0x1F, 0x0C, 0x1F }, /* 'W' */
	{ 0x1B, 0x04, 0x1B }, /* 'X' */
	{ 0x03, 0x1C, 0x03 }, /* 'Y' */
	{ 0x19, 0x15, 0x13 }, /* 'Z' */
	{ 0x00, 0x1F, 0x11 }, /* '[' */
	{ 0x03, 0x04, 0x18 }, /* '\' */
	{ 0x11, 0x1F, 0x00 }, /* ']' */
	{ 0x02, 0x01, 0x02 }, /* '^' */
	{ 0x10, 0x10, 0x10 }, /* '_' */
	{ 0x01, 0x02, 0x00 }, /* '`' */
	{ 0x1A, 0x16, 0x1C }, /* 'a' */
	{ 0x1F, 0x12, 0x0C }, /* 'b' */
	{ 0x0C, 0x12, 0x12 }, /* 'c' */
	{ 0x0C, 0x12, 0x1F }, /* 'd' */
	{ 0x0C, 0x1A, 0x16 }, /* 'e' */
	{ 0x04, 0x1E, 0x05 }, /* 'f' */
	{ 0x2C, 0x2A, 0x16 }, /* 'g' */
	{ 0x1F, 0x02, 0x1C }, /* 'h' */
	{ 0x00, 0x1D, 0x00 }, /* 'i' */
	{ 0x10, 0x20, 0x1D }, /* 'j' */
	{ 0x1F, 0x08, 0x14 }, /* 'k' */
	{ 0x11, 0x1E, 0x10 }, /* 'l' */
	{ 0x1E, 0x0E, 0x1E }, /* 'm' */
	{ 0x1E, 0x02, 0x1C }, /* 'n' */
	{ 0x1C, 0x12, 0x0E }, /* 'o' */
	{ 0x3E, 0x12, 0x0C }, /* 'p' */
	{ 0x0C, 0x12, 0x3E }, /* 'q' */
	{ 0x1C, 0x02, 0x02 }, /* 'r' */
	{ 0x14, 0x1E, 0x0A }, /* 's' */
	{ 0x02, 0x0F, 0x12 }, /* 't' */
	{ 0x0E, 0x10, 0x1E }, /* 'u' */
	{ 0x06, 0x18, 0x06 }, /* 'v' */
	{ 0x1E, 0x1C, 0x1E }, /* 'w' */
	{ 0x12, 0x0C, 0x12 }, /* 'x' */
	{ 0x26, 0x28, 0x1E }, /* 'y' */
	{ 0x1A, 0x1E, 0x16 }, /* 'z' */
	{ 0x04, 0x1B, 0x11 }, /* '{' */
	{ 0x00, 0x1F, 0x00 }, /* '|' */
	{ 0x11, 0x1B, 0x04 }, /* '}' */
	{ 0x0C, 0x04, 0x06 }, /* '~' */
};
