#include "codeeditorform.h"
#include "ui_codeeditorform.h"

#include <QToolTip>
#include <QMenu>
#include <QPixmap>

#include "Qsci/qsciscintillabase.h"

#include "main.h"

#include "ccc65interface.h"

#include "cdockwidgetregistry.h"

#include "dbg_cnes6502.h"

#include "cbreakpointinfo.h"
#include "cmarker.h"

enum
{
   Margin_Decorations = 0,
   Margin_LineNumbers
};

enum
{
   Marker_Breakpoint = 0,
   Marker_BreakpointDisabled,
   Marker_Execution,
   Marker_Error,
   Marker_Highlight
};

CodeEditorForm::CodeEditorForm(QString fileName,QString sourceCode,IProjectTreeViewItem* link,QWidget* parent) :
   CDesignerEditorBase(link,parent),
   ui(new Ui::CodeEditorForm)
{
   QDockWidget* codeBrowser = CDockWidgetRegistry::getWidget("Code Browser");
   QDockWidget* breakpoints = CDockWidgetRegistry::getWidget("Breakpoints");

   ui->setupUi(this);

   m_scintilla = new QsciScintilla();

   m_scintilla->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
   m_scintilla->setMarginWidth(2,0);
   m_scintilla->setMarginMarkerMask(2,0);
   m_scintilla->setMarginWidth(3,0);
   m_scintilla->setMarginMarkerMask(3,0);
   m_scintilla->setMarginWidth(4,0);
   m_scintilla->setMarginMarkerMask(4,0);

   m_scintilla->setMarginWidth(Margin_Decorations,22);
   m_scintilla->setMarginMarkerMask(Margin_Decorations,0x0000000F);
   m_scintilla->setMarginType(Margin_Decorations,QsciScintilla::SymbolMargin);
   m_scintilla->setMarginSensitivity(Margin_Decorations,true);

   m_scintilla->setMarginLineNumbers(Margin_LineNumbers,true);
   m_scintilla->setMarginWidth(Margin_LineNumbers,0);
   m_scintilla->setMarginMarkerMask(Margin_LineNumbers,0);
   m_scintilla->setMarginType(Margin_LineNumbers,QsciScintilla::NumberMargin);
   m_scintilla->setMarginSensitivity(Margin_LineNumbers,true);

   m_scintilla->setSelectionBackgroundColor(QColor(215,215,215));
   m_scintilla->setSelectionToEol(true);

   m_scintilla->markerDefine(QPixmap(":/resources/22_execution_pointer.png"),Marker_Execution);
   m_scintilla->markerDefine(QPixmap(":/resources/22_breakpoint.png"),Marker_Breakpoint);
   m_scintilla->markerDefine(QPixmap(":/resources/22_breakpoint_disabled.png"),Marker_BreakpointDisabled);
   m_scintilla->markerDefine(QsciScintilla::CircledMinus,Marker_Error);
   m_scintilla->setMarkerForegroundColor(QColor(255,255,0),Marker_Error);
   m_scintilla->setMarkerBackgroundColor(QColor(255,0,0),Marker_Error);
   m_scintilla->markerDefine(QsciScintilla::Background,Marker_Highlight);
   m_scintilla->setMarkerBackgroundColor(QColor(235,235,235),Marker_Highlight);

   m_lexer = new QsciLexerCA65(m_scintilla);
   m_scintilla->setLexer(m_lexer);

   QObject::connect(m_scintilla,SIGNAL(marginClicked(int,int,Qt::KeyboardModifiers)),this,SLOT(editor_marginClicked(int,int,Qt::KeyboardModifiers)));
   QObject::connect(m_scintilla,SIGNAL(linesChanged()),this,SLOT(editor_linesChanged()));
   QObject::connect(m_scintilla,SIGNAL(modificationChanged(bool)),this,SLOT(editor_modificationChanged(bool)));

   ui->gridLayout->addWidget(m_scintilla);

   QObject::connect(codeBrowser,SIGNAL(breakpointsChanged()),this,SLOT(external_breakpointsChanged()) );

   // Connect signals to the UI to have the UI update.
//   QObject::connect ( emulator, SIGNAL(cartridgeLoaded()), this, SLOT(repaint()) );
//   QObject::connect ( emulator, SIGNAL(emulatorReset()), this, SLOT(repaint()) );
//   QObject::connect ( emulator, SIGNAL(emulatorPaused(bool)), this, SLOT(repaint()) );
//   QObject::connect ( emulator, SIGNAL(emulatorStarted()), this, SLOT(repaint()) );
   QObject::connect ( breakpointWatcher, SIGNAL(breakpointHit()), this,SLOT(breakpointHit()) );

   QObject::connect ( this, SIGNAL(breakpointsChanged()), breakpoints, SIGNAL(breakpointsChanged()) );

   QObject::connect ( breakpoints, SIGNAL(breakpointsChanged()), this, SLOT(external_breakpointsChanged()) );

   QObject::connect ( compiler, SIGNAL(compileStarted()), this, SLOT(compiler_compileStarted()) );
   QObject::connect ( compiler, SIGNAL(compileDone(bool)), this, SLOT(compiler_compileDone(bool)) );

   QObject::connect ( emulator, SIGNAL(emulatorStarted()), this, SLOT(emulator_emulatorStarted()) );

   // Finally set the text in the Scintilla object.
   setSourceCode(sourceCode);

   m_fileName = fileName;
}

CodeEditorForm::~CodeEditorForm()
{
   delete ui;

   delete m_lexer;
   delete m_scintilla;
}

void CodeEditorForm::changeEvent(QEvent* e)
{
   QWidget::changeEvent(e);

   switch (e->type())
   {
      case QEvent::LanguageChange:
         ui->retranslateUi(this);
         break;
      default:
         break;
   }
}

void CodeEditorForm::compiler_compileStarted()
{
   m_scintilla->markerDeleteAll(Marker_Error);
}

void CodeEditorForm::compiler_compileDone(bool ok)
{
   int line;

   for ( line = 0; line < m_scintilla->lines(); line++ )
   {
      if ( CCC65Interface::isErrorOnLineOfFile(m_fileName,line+1) )
      {
         m_scintilla->markerAdd(line,Marker_Error);
      }
   }
}

void CodeEditorForm::emulator_emulatorStarted()
{
   m_scintilla->markerDeleteAll(Marker_Execution);
   m_scintilla->markerDeleteAll(Marker_Highlight);
}

void CodeEditorForm::external_breakpointsChanged()
{
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();
   CMarker* markers = nesGetExecutionMarkerDatabase();
   MarkerSetInfo* pMarker;
   int addr;
   int absAddr;
   int line;
   int index;
   int idx;

   m_scintilla->getCursorPosition(&line,&index);

   m_scintilla->markerDeleteAll(Marker_Breakpoint);
   m_scintilla->markerDeleteAll(Marker_BreakpointDisabled);

   for ( line = 0; line < m_scintilla->lines(); line++ )
   {
      addr = CCC65Interface::getAddressFromFileAndLine(m_fileName,line+1);

      absAddr = CCC65Interface::getAbsoluteAddressFromFileAndLine(m_fileName,line+1);

      for ( idx = 0; idx < markers->GetNumMarkers(); idx++ )
      {
         pMarker = markers->GetMarker(idx);

         if ( (pMarker->state == eMarkerSet_Started) ||
              (pMarker->state == eMarkerSet_Complete) )
         {
            if ( (absAddr >= pMarker->startAbsAddr) &&
                 (absAddr <= pMarker->endAbsAddr) )
            {
            }
         }
      }

      for ( idx = 0; idx < pBreakpoints->GetNumBreakpoints(); idx++ )
      {
         BreakpointInfo* pBreakpoint = pBreakpoints->GetBreakpoint(idx);

         if ( (pBreakpoint->enabled) &&
              (pBreakpoint->type == eBreakOnCPUExecution) &&
              (pBreakpoint->item1 <= addr) &&
              ((absAddr == -1) || (absAddr == pBreakpoint->item1Absolute)) )
         {
            m_scintilla->markerAdd(line,Marker_Breakpoint);
         }
         else if ( (!pBreakpoint->enabled) &&
                   (pBreakpoint->type == eBreakOnCPUExecution) &&
                   (pBreakpoint->item1 <= addr) &&
                   ((absAddr == -1) || (absAddr == pBreakpoint->item1Absolute)) )
         {
            m_scintilla->markerAdd(line,Marker_BreakpointDisabled);
         }
      }
   }
}

void CodeEditorForm::breakpointHit()
{

}

void CodeEditorForm::editor_modificationChanged(bool m)
{
}

void CodeEditorForm::editor_linesChanged()
{
   QString maxLineNum;

   maxLineNum.sprintf("%d",m_scintilla->lines());

   m_scintilla->setMarginWidth(Margin_LineNumbers,maxLineNum);
}

void CodeEditorForm::editor_marginClicked(int margin,int line,Qt::KeyboardModifiers modifiers)
{
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();
   int bp;
   int addr = 0;
   int absAddr = 0;

   m_scintilla->setCursorPosition(line,0);

   addr = CCC65Interface::getAddressFromFileAndLine(m_fileName,line+1);

   absAddr = CCC65Interface::getAbsoluteAddressFromFileAndLine(m_fileName,line+1);

   if ( addr != -1 )
   {
      bp = pBreakpoints->FindExactMatch ( eBreakOnCPUExecution,
                                          eBreakpointItemAddress,
                                          0,
                                          addr,
                                          absAddr,
                                          addr,
                                          eBreakpointConditionNone,
                                          0,
                                          eBreakpointDataNone,
                                          0 );

      m_breakpointIndex = bp;

      // If breakpoint isn't set here, give menu options to set one...
      if ( bp < 0 )
      {
         on_actionBreak_on_CPU_execution_here_triggered();
      }
      else
      {
         if ( pBreakpoints->GetStatus(bp) == Breakpoint_Disabled )
         {
            on_actionRemove_breakpoint_triggered();

            m_scintilla->markerDelete(line,Marker_BreakpointDisabled);
         }
         else
         {
            on_actionDisable_breakpoint_triggered();

            m_scintilla->markerDelete(line,Marker_Breakpoint);
            m_scintilla->markerAdd(line,Marker_BreakpointDisabled);
         }
      }

      emit breakpointsChanged();
   }
}

void CodeEditorForm::contextMenuEvent(QContextMenuEvent *e)
{
   QMenu menu;
//   QMenu *pMenu = m_scintilla->createStandardContextMenu();
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();
   int bp;
   int line;
   int index;
   int addr = 0;
   int absAddr = 0;

   m_scintilla->getCursorPosition(&line,&index);

   addr = CCC65Interface::getAddressFromFileAndLine(m_fileName,line+1);

   absAddr = CCC65Interface::getAbsoluteAddressFromFileAndLine(m_fileName,line+1);

   if ( addr != -1 )
   {
      bp = pBreakpoints->FindExactMatch ( eBreakOnCPUExecution,
                                          eBreakpointItemAddress,
                                          0,
                                          addr,
                                          absAddr,
                                          addr,
                                          eBreakpointConditionNone,
                                          0,
                                          eBreakpointDataNone,
                                          0 );

      // Build context menu...
      menu.addAction(ui->actionRun_to_here);
      menu.addSeparator();

      // If breakpoint isn't set here, give menu options to set one...
      if ( bp < 0 )
      {
         menu.addAction(ui->actionBreak_on_CPU_execution_here);
      }
      else
      {
         if ( pBreakpoints->GetStatus(bp) == Breakpoint_Disabled )
         {
            menu.addAction(ui->actionEnable_breakpoint);
            menu.addAction(ui->actionRemove_breakpoint);
         }
         else
         {
            menu.addAction(ui->actionDisable_breakpoint);
            menu.addAction(ui->actionRemove_breakpoint);
         }
      }

      menu.addSeparator();
      menu.addAction(ui->actionClear_marker);
      menu.addSeparator();

      menu.addAction(ui->actionStart_marker_here);
      menu.addAction(ui->actionEnd_marker_here);
      menu.addSeparator();

//      menu.addActions(pMenu->actions());

      m_breakpointIndex = bp;
   }
   else
   {
//      menu.addActions(pMenu->actions());
   }

   // Run the context menu...
   menu.exec(e->globalPos());

//   delete pMenu;
}


void CodeEditorForm::on_actionBreak_on_CPU_execution_here_triggered()
{
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();
   int bpIdx;
   int line;
   int index;
   int addr = 0;
   int absAddr = 0;

   m_scintilla->getCursorPosition(&line,&index);

   addr = CCC65Interface::getAddressFromFileAndLine(m_fileName,line+1);

   absAddr = CCC65Interface::getAbsoluteAddressFromFileAndLine(m_fileName,line+1);

   if ( addr != -1 )
   {
      bpIdx = pBreakpoints->AddBreakpoint ( eBreakOnCPUExecution,
                                            eBreakpointItemAddress,
                                            0,
                                            addr,
                                            absAddr,
                                            addr,
                                            eBreakpointConditionNone,
                                            0,
                                            eBreakpointDataNone,
                                            0,
                                            true );

      if ( bpIdx < 0 )
      {
         QString str;
         str.sprintf("Cannot add breakpoint, maximum of %d already used.", NUM_BREAKPOINTS);
         QMessageBox::information(0, "Error", str);
      }
      else
      {
         m_scintilla->markerAdd(line,Marker_Breakpoint);
      }

      emit breakpointsChanged();
   }
}

void CodeEditorForm::on_actionRun_to_here_triggered()
{
#if 0
   int addr = 0;
   int absAddr = 0;

   addr = pasm_get_source_addr_by_linenum_and_file ( m_ctxtTextCursor.blockNumber()+1, ui->textEdit->documentTitle().toAscii().constData() );

   absAddr = pasm_get_source_absolute_addr_by_linenum_and_file ( m_ctxtTextCursor.blockNumber()+1, ui->textEdit->documentTitle().toAscii().constData() );

   if ( addr != -1 )
   {
//      C6502DBG::GOTO(addr);
   }// CPTODO: fix the goto for absolute
#endif
}

void CodeEditorForm::on_actionDisable_breakpoint_triggered()
{
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();

   if ( m_breakpointIndex >= 0 )
   {
      pBreakpoints->ToggleEnabled(m_breakpointIndex);
   }
}

void CodeEditorForm::on_actionRemove_breakpoint_triggered()
{
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();

   if ( m_breakpointIndex >= 0 )
   {
      pBreakpoints->RemoveBreakpoint(m_breakpointIndex);
   }
}

void CodeEditorForm::on_actionEnable_breakpoint_triggered()
{
   CBreakpointInfo* pBreakpoints = nesGetBreakpointDatabase();

   if ( m_breakpointIndex >= 0 )
   {
      pBreakpoints->ToggleEnabled(m_breakpointIndex);
   }
}

void CodeEditorForm::on_actionStart_marker_here_triggered()
{
   CMarker* markers = nesGetExecutionMarkerDatabase();
   int marker;
   int line;
   int index;
   int addr = 0;
   int absAddr = 0;

   m_scintilla->getCursorPosition(&line,&index);

   addr = CCC65Interface::getAddressFromFileAndLine(m_fileName,line+1);

   absAddr = CCC65Interface::getAbsoluteAddressFromFileAndLine(m_fileName,line+1);

   if ( addr != -1 )
   {
      // Find unused Marker entry...
      marker = markers->AddMarker(absAddr);
   }
}

void CodeEditorForm::on_actionEnd_marker_here_triggered()
{
   CMarker* markers = nesGetExecutionMarkerDatabase();
   int marker = markers->FindInProgressMarker();
   int line;
   int index;
   int addr = 0;
   int absAddr = 0;

   if ( marker >= 0 )
   {
      m_scintilla->getCursorPosition(&line,&index);

      addr = CCC65Interface::getAddressFromFileAndLine(m_fileName,line+1);

      absAddr = CCC65Interface::getAbsoluteAddressFromFileAndLine(m_fileName,line+1);

      if ( addr != -1 )
      {
         markers->CompleteMarker(marker,nesGetAbsoluteAddressFromAddress(addr));
      }
   }
}

void CodeEditorForm::on_actionClear_marker_triggered()
{
   CMarker* markers = nesGetExecutionMarkerDatabase();
   markers->ClearAllMarkers();
}

QString CodeEditorForm::sourceCode()
{
   return m_scintilla->text();
}

void CodeEditorForm::setSourceCode(QString source)
{
   m_scintilla->setText(source);

   // Force repaint of breakpoints since the reason this API is
   // called is usually when a CodeEditorForm is opened for the
   // first time or subsequent times after being closed.  Any
   // breakpoints set in this code module would otherwise disappear
   // on subsequent opens.  (They're still in the breakpoint database
   // they just wouldn't show up in the code editor).
   external_breakpointsChanged();

   // Force repaint of error tags.
   compiler_compileDone(true);

   // Setting the text of the Scintilla object unfortunately marks
   // it as "modified".  Reset our modified flag.
   setModified(false);
}

void CodeEditorForm::selectLine(int linenumber)
{
   if ( m_scintilla )
   {
      m_scintilla->markerDeleteAll(Marker_Execution);
      m_scintilla->markerDeleteAll(Marker_Highlight);
      if ( linenumber >= 0 )
      {
         m_scintilla->ensureLineVisible(linenumber-1);
         m_scintilla->markerAdd(linenumber-1,Marker_Execution);
         m_scintilla->markerAdd(linenumber-1,Marker_Highlight);
      }
   }
}

void CodeEditorForm::highlightLine(int linenumber)
{
   if ( m_scintilla )
   {
      m_scintilla->markerDeleteAll(Marker_Highlight);
      if ( linenumber >= 0 )
      {
         m_scintilla->ensureLineVisible(linenumber-1);
         m_scintilla->markerAdd(linenumber-1,Marker_Highlight);
      }
   }
}
