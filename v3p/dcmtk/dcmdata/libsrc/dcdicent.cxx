/*
 *
 *  Copyright (C) 1994-2002, OFFIS
 *
 *  This software and supporting documentation were developed by
 *
 *    Kuratorium OFFIS e.V.
 *    Healthcare Information and Communication Systems
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *  THIS SOFTWARE IS MADE AVAILABLE,  AS IS,  AND OFFIS MAKES NO  WARRANTY
 *  REGARDING  THE  SOFTWARE,  ITS  PERFORMANCE,  ITS  MERCHANTABILITY  OR
 *  FITNESS FOR ANY PARTICULAR USE, FREEDOM FROM ANY COMPUTER DISEASES  OR
 *  ITS CONFORMITY TO ANY SPECIFICATION. THE ENTIRE RISK AS TO QUALITY AND
 *  PERFORMANCE OF THE SOFTWARE IS WITH THE USER.
 *
 *  Module:  dcmdata
 *
 *  Author:  Andrew Hewett
 *
 *  Purpose: a dictionary entry in the loadable DICOM data dictionary
 *
 */

#include "osconfig.h"    /* make sure OS specific configuration is included first */

#define INCLUDE_CSTDIO
#define INCLUDE_CSTRING
#include "ofstdinc.h"

#include "dcdicent.h"

/*
** DcmDictEntry member functions
*/

static
char* strdup_new(const char* str)
{
    char* s = NULL;
    if (str != NULL) {
        s = new char[strlen(str)+1];
        strcpy(s, str);
    }
    return s;
}

DcmDictEntry::DcmDictEntry(Uint16 g, Uint16 e, DcmVR vr,
        const char* nam, int vmMin, int vmMax,
        const char* vers, OFBool doCopyStrings,
        const char* pcreator)
  : DcmTagKey(g,e),
    upperKey(),
    valueRepresentation(EVR_UNKNOWN),
    tagName(nam),
    valueMultiplicityMin(vmMin),
    valueMultiplicityMax(vmMax),
    standardVersion(vers),
    stringsAreCopies(doCopyStrings),
    groupRangeRestriction(DcmDictRange_Unspecified),
    elementRangeRestriction(DcmDictRange_Unspecified),
    privateCreator(pcreator)
{
    upperKey.set(g,e);  /* default: make upper key same as normal key */
    valueRepresentation.setVR(vr);
    if (doCopyStrings) {
        tagName = strdup_new(nam);
        standardVersion = strdup_new(vers);
        privateCreator = strdup_new(pcreator);
    }
}

DcmDictEntry::DcmDictEntry(Uint16 g, Uint16 e, Uint16 ug, Uint16 ue, DcmVR vr,
        const char* nam, int vmMin, int vmMax,
        const char* vers, OFBool doCopyStrings,
        const char* pcreator)
  : DcmTagKey(g,e),
    upperKey(),
    valueRepresentation(EVR_UNKNOWN),
    tagName(nam),
    valueMultiplicityMin(vmMin),
    valueMultiplicityMax(vmMax),
    standardVersion(vers),
    stringsAreCopies(doCopyStrings),
    groupRangeRestriction(DcmDictRange_Unspecified),
    elementRangeRestriction(DcmDictRange_Unspecified),
    privateCreator(pcreator)
{
    upperKey.set(ug, ue);
    valueRepresentation.setVR(vr);
    if (doCopyStrings) {
        tagName = strdup_new(nam);
        standardVersion = strdup_new(vers);
        privateCreator = strdup_new(pcreator);
    }
}

DcmDictEntry::DcmDictEntry(const DcmDictEntry& e)
  : DcmTagKey(e),
    upperKey(e.upperKey),
    valueRepresentation(e.valueRepresentation),
    tagName(e.tagName),
    valueMultiplicityMin(e.valueMultiplicityMin),
    valueMultiplicityMax(e.valueMultiplicityMax),
    standardVersion(e.standardVersion),
    stringsAreCopies(e.stringsAreCopies),
    groupRangeRestriction(e.groupRangeRestriction),
    elementRangeRestriction(e.elementRangeRestriction),
    privateCreator(e.privateCreator)
{
    if (stringsAreCopies)
    {
        tagName = strdup_new(e.tagName);
        standardVersion = strdup_new(e.standardVersion);
        privateCreator = strdup_new(e.privateCreator);
    }
}


DcmDictEntry::~DcmDictEntry()
{
    if (stringsAreCopies) {
        /* we have allocated them so it is ok to deallocate them */
        delete[] (char*)tagName;
        delete[] (char*)standardVersion;
        delete[] (char*)privateCreator;
    }
}

/*
** DcmDictEntry friend functions
*/

ostream& operator<<(ostream& s, const DcmDictEntry& e)
{
    const DcmDictEntry* ep = &e;
    if (ep == NULL) {
        s << "(nil)";
        return s;
    }

    char tagBuf[16];

    sprintf(tagBuf, "(%04x", e.getGroup());
    s << tagBuf;
    if (e.isRepeatingGroup()) {
        sprintf(tagBuf, "-%04x", e.getUpperGroup());
        s << tagBuf;
    }
    sprintf(tagBuf, ",%04x", e.getElement());
    s << tagBuf;
    if (e.isRepeatingElement()) {
        sprintf(tagBuf, "-%04x", e.getUpperElement());
        s << tagBuf;
    }
    s << ")";


    s << " " << e.getVR().getVRName() << " \"" << e.getTagName() << "\" ";
    if (e.isFixedSingleVM()) {
        s << "vm=" << e.getVMMax() << " ";
    } else if (e.isVariableRangeVM()) {
        s << "vm=" << e.getVMMin() << "-n ";
    } else if (e.isFixedRangeVM()){
        s << "vm=" << e.getVMMin() << "-" << e.getVMMax() << " ";
    } else {
        s << "vm=?(" <<  e.getVMMin() << "-" << e.getVMMax() << ")? ";
    }
    if (e.getStandardVersion() != NULL) {
        s << " Version=\"" << e.getStandardVersion() << "\" ";
    }
    if (e.getPrivateCreator() != NULL) {
        s << " priv=\"" << e.getPrivateCreator() << "\" ";
    }

    return s;
}
