/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   knob_png;
    const int            knob_pngSize = 164079;

    extern const char*   display_png;
    const int            display_pngSize = 1096229;

    extern const char*   redbutton_png;
    const int            redbutton_pngSize = 19119;

    extern const char*   orangebutton_png;
    const int            orangebutton_pngSize = 19870;

    extern const char*   greenbutton_png;
    const int            greenbutton_pngSize = 19011;

    extern const char*   bluebutton_png;
    const int            bluebutton_pngSize = 19920;

    extern const char*   purplebutton_png;
    const int            purplebutton_pngSize = 20613;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 7;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
