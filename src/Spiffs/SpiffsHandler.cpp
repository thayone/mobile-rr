
#include "SpiffsHandler.h"

void SpiffsHandler::setupSPIFFS() {

    FSInfo fs_info;  // Info about SPIFFS
    Dir dir;         // Directory struct for SPIFFS
    File f;          // Filehandle
    String filename; // Name of file found in SPIFFS

    // Enable file system
    SPIFFS.begin();

    // Show some info about the SPIFFS
    uint16_t cnt = 0;
    SPIFFS.info ( fs_info );


/*
    dbg_printf ( "SPIFFS Files\nName                           -      Size" );
    // Show files in FS
    dir = SPIFFS.openDir ( "/" );

    // All files
    while ( dir.next() )
    {
        f = dir.openFile ( "r" );
        filename = dir.fileName();
        // Show name and size
        dbg_printf ( "%-30s - %9s",
                     filename.c_str(),
                     formatBytes ( f.size() ).c_str()
                   );
        cnt++;
    }

    dbg_printf ( "%d Files, %s of %s Used",
                 cnt,
                 formatBytes ( fs_info.usedBytes ).c_str(),
                 formatBytes ( fs_info.totalBytes ).c_str()
               );
    dbg_printf ( "%s Free\n",
                 formatBytes ( fs_info.totalBytes - fs_info.usedBytes ).c_str()
               );

*/


}

