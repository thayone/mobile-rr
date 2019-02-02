
#include <string.h>
#include <Arduino.h>

#ifndef _HELPER_H_
#define _HELPER_H_


//***************************************************************************
//                            D B G P R I N T                               *
//***************************************************************************
void dbg_printf ( const char *format, ... )
{
    static char sbuf[1400];                               // For debug lines
    va_list varArgs;                                      // For variable number of params

    va_start ( varArgs, format );                         // Prepare parameters
    vsnprintf ( sbuf, sizeof ( sbuf ), format, varArgs ); // Format the message
    va_end ( varArgs );                                   // End of using parameters

    Serial.println ( sbuf );
}


//***************************************************************************
//                    F O R M A T  B Y T E S                                *
//***************************************************************************
String formatBytes ( size_t bytes )
{
    if ( bytes < 1024 )
    {
        return String ( bytes ) + " B";
    }
    else if ( bytes < ( 1024 * 1024 ) )
    {
        return String ( bytes / 1024.0 ) + " KB";
    }
    else if ( bytes < ( 1024 * 1024 * 1024 ) )
    {
        return String ( bytes / 1024.0 / 1024.0 ) + " MB";
    }
    else
    {
        return String ( bytes / 1024.0 / 1024.0 / 1024.0 ) + " GB";
    }
}



#endif