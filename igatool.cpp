// exiga.cpp, v1.0 2011/05/20
// coded by asmodean

// contact:
//   web:   http://asmodean.reverse.net
//   email: asmodean [at] hush.com
//   irc:   asmodean on efnet (irc.efnet.net)

// Modified by Fuyin
// 2014/09/07

// Modified again by Neroldy
// 2016/06/25
// 2017/09/25

// Fucked agin again by Lipsum
// 2021/02/23

// This tool extracts and rebuild IGA0 (*.iga) archives.
// If the iga file is large, such as bgimage.iga, please compile 64bit version.

#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
using namespace std;

struct IGAHDR
{
    char signature[4];       // "IGA0"
    uint32_t unknown1;  // it seems like a checksum
    uint32_t unknown2;  // 02 00 00 00
    uint32_t unknown3;  // 02 00 00 00
};

struct IGAENTRY
{
    unsigned long filename_offset;
    unsigned long offset;
    unsigned long length;
};

// 0F4FAEA0    0FB655 00       movzx   edx, byte ptr[ebp]; ebp = src
// 0F4FAEA4    C1E3 07         shl     ebx, 0x7
// 0F4FAEA7    0BDA            or      ebx, edx
// 0F4FAEA9    F6C3 01         test    bl, 0x1
unsigned long get_multibyte_long(unsigned char*& buff)
{
    unsigned long v = 0;

    while (!(v & 1))
    {
        v = (v << 7) | *buff++;
    }

    return v >> 1;
}

vector<unsigned char> cal_multibyte(unsigned long val)
{
    vector<unsigned char> rst;
    unsigned long long v = val;

    if (v == 0)
    {
        rst.push_back(0x01);
        return rst;
    }

    v = (v << 1) + 1;
    unsigned char cur_byte = v & 0xFF;
    while (v & 0xFFFFFFFFFFFFFFFE)
    {
        rst.push_back(cur_byte);
        v >>= 7;
        cur_byte = v & 0xFE;
    }

    reverse(rst.begin(), rst.end());
    return rst;
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 4)
    {
        fprintf(stderr, "igatool v1.2 by asmodean & modified by Fuyin & modified again by Neroldy\n");
        fprintf(stderr, "usage(unpack mode): %s <input.iga> [-x]\n", argv[0]);
        fprintf(stderr, "example: igatool.exe fgimage.iga\n");
        fprintf(stderr, "Lipsum: because of fucking Microsoft, pack mode is gone.\n");
        fprintf(stderr, "option [-x] is for script xor encrypt or decrypt\n");
        return -1;
    }
    // xor_flag for script encrypt or decrypt
    bool xor_flag = false;
    string in_filename(argv[1]);
    string in_foldername;
    string out_filename;
    int fd = -1;
    int fout = -1;
    vector<string> in_fileslist;
    if (argc == 2 || (argc == 3 && strcmp(argv[2], "-x") == 0))  // read
    {
        if (argc == 3) xor_flag = true;
        fd = open(in_filename.c_str(), O_RDONLY);
        IGAHDR hdr;
        read(fd, &hdr, sizeof(hdr));
        // "big enough" and read the iga file entry block
        unsigned long toc_len = 1024 * 1024 * 5;
        unsigned char* toc_buff = new unsigned char[toc_len];
        read(fd, toc_buff, toc_len);

        unsigned char* p = toc_buff;
        unsigned long entries_len = get_multibyte_long(p);
        unsigned char* end = p + entries_len;
        typedef vector<IGAENTRY> entries_t;
        entries_t entries;

        while (p < end)
        {
            IGAENTRY entry;

            entry.filename_offset = get_multibyte_long(p);
            entry.offset = get_multibyte_long(p);
            entry.length = get_multibyte_long(p);

            entries.push_back(entry);
        }

        unsigned long filenames_len = get_multibyte_long(p);
        struct stat buf;
        fstat(fd, &buf);
        unsigned long data_base = buf.st_size - (entries.rbegin()->offset + entries.rbegin()->length);

        // create new folder which has the same name as the iga file and put the file in it
        string out_folder = in_filename.substr(0, in_filename.length() - 4);
        mkdir(out_folder.c_str(), 0755);

        for (auto i = entries.begin(); i != entries.end(); ++i)
        {
            auto next = i + 1;
            unsigned long name_len =
                (next == entries.end() ? filenames_len : next->filename_offset) - i->filename_offset;

            string filename;

            while (name_len--)
            {
                filename += (char)get_multibyte_long(p);
            }

            unsigned long len = i->length;
            unsigned char* buff = new unsigned char[len];
            lseek(fd, data_base + i->offset, SEEK_SET);
            read(fd, buff, len);

            for (unsigned long j = 0; j < len; j++)
            {
                buff[j] ^= (unsigned char)(j + 2);
                if (xor_flag) buff[j] ^= 0xFF;
            }
            string outputfile = out_folder + "/" + filename;
            int file = open(outputfile.c_str(), O_RDWR | O_CREAT | O_TRUNC);
            fchmod(file, 0644);
            write(file, buff, len);
            close(file);

            delete[] buff;
        }
        delete[] toc_buff;
        close(fd);
    }
    return 0;
}
