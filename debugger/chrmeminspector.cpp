#include "chrmeminspector.h"
#include "cnesppu.h"

#include "cnesppu.h"

CHRMEMInspector::CHRMEMInspector()
{
   frame = new QFrame ( this );
   frame->setFrameRect ( this->rect() );

   dialog = new CHRROMDisplayDialog ( frame, true, NULL );
   this->setWidget ( dialog );
}

CHRMEMInspector::~CHRMEMInspector()
{
   delete dialog;
   delete frame;
}