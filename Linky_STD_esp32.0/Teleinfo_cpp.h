
// **********************************************************************************
// Driver definition for French Teleinfo
// **********************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this library but please abide with the CC-BY-SA license:
// http://creativecommons.org/licenses/by-sa/4.0/
//
// For any explanation about teleinfo ou use , see my blog
// http://hallard.me/category/tinfo
//
// Code based on following datasheet
// http://www.erdf.fr/sites/default/files/ERDF-NOI-CPT_02E.pdf
//
// Written by Charles-Henri Hallard (http://hallard.me)
//
// History : V1.00 2015-06-14 - First release
//
// All text above must be included in any redistribution.
//
// Edit : Tab size set to 2 but I converted tab to sapces
//
// **********************************************************************************
//
// Adaptation au mode "Standart"  par A.Durand Octobre 2020
// la grosse difference et le remplacement du séparateur " " par '\t' et
// l'ajout du champ horodatage sur certain message.
// Fonctionne correctement sur Esp32,
// sur Arduino Uno probleme de manque de memoire pour les 38 datas de la TIC du Linky 
// les champs vides de la TIC "Value" ou "Horodatage" sont force a "*" car je m'arrive pas 
//sur le Esp32 a gérer correctement les champs NULL. 
//
// ********************************************************************************** 

/* ======================================================================
Class   : TInfo
Purpose : Constructor
Input   : -
Output  : -
Comments: -
====================================================================== */
TInfo::TInfo()
{
  // Init of our linked list
  _valueslist.name = NULL;
  _valueslist.value = NULL;
  _valueslist.checksum = '\0';
  _valueslist.flags = TINFO_FLAGS_NONE;

  // callback
  _fn_ADPS = NULL;
  _fn_data = NULL;   
  _fn_new_frame = NULL;   
  _fn_updated_frame = NULL;   
}

/* ======================================================================
Function: init
Purpose : configure ULPNode I/O ports 
Input   : -
Output  : -
Comments: - 
====================================================================== */
void TInfo::init()
{
  // free up linked list (in case on recall init())
  listDelete();

  // clear our receive buffer
  clearBuffer();

  // We're in INIT in term of receive data
  _state = TINFO_INIT;
}

/* ======================================================================
Function: attachADPS 
Purpose : attach a callback when we detected a ADPS on any phase
Input   : callback function
Output  : - 
Comments: -
====================================================================== */
void TInfo::attachADPS(void (*fn_ADPS)(uint8_t phase))
{
  // indicate the user callback
  _fn_ADPS = fn_ADPS;   
}

/* ======================================================================
Function: attachNewData 
Purpose : attach a callback when we detected a new/changed value 
Input   : callback function
Output  : - 
Comments: -
====================================================================== */
void TInfo::attachData(void (*fn_data)(ValueList * valueslist, uint8_t  state))
{
  // indicate the user callback
  _fn_data = fn_data;   
}

/* ======================================================================
Function: attachNewFrame 
Purpose : attach a callback when we received a full frame
Input   : callback function
Output  : - 
Comments: -
====================================================================== */
void TInfo::attachNewFrame(void (*fn_new_frame)(ValueList * valueslist))
{
  // indicate the user callback
  _fn_new_frame = fn_new_frame;   
}

/* ======================================================================
Function: attachChangedFrame 
Purpose : attach a callback when we received a full frame where data
          has changed since the last frame (cool to update data)
Input   : callback function
Output  : - 
Comments: -
====================================================================== */
void TInfo::attachUpdatedFrame(void (*fn_updated_frame)(ValueList * valueslist))
{
  // indicate the user callback
  _fn_updated_frame = fn_updated_frame;   
}

/* ======================================================================
Function: clearBuffer
Purpose : clear and init the buffer
Input   : -
Output  : -
Comments: - 
====================================================================== */
void TInfo::clearBuffer()
{
  // Clear our buffer, set index to 0
  memset(_recv_buff, 0, TINFO_BUFSIZE);
  _recv_idx = 0;
}


/* ======================================================================
Function: addCustomValue
Purpose : let user add custom values (mainly for testing)
Input   : Pointer to the label name
          pointer to the value
          pointer on flag state of the label 
Output  : pointer to the new node (or founded one)
Comments: checksum is calculated before adding, no need to bother with
====================================================================== */
ValueList * TInfo::addCustomValue(char * name, char * value, char * date, uint8_t * flags)
{
  // Little check
  if (name && *name && value && *value) {
    ValueList * me;

    // Same as if we really received this line
    customLabel(name, value, flags);
    me = valueAdd(name, value, date, calcChecksum(name,value,date), flags);

    if ( me ) {
      // something to do with new datas
      if (*flags & (TINFO_FLAGS_UPDATED | TINFO_FLAGS_ADDED | TINFO_FLAGS_ALERT) ) {
        // this frame will for sure be updated
        _frame_updated = true;
      }
      return (me);
    }
  }

  // Error or Already Exists
  return ( (ValueList *) NULL);
}

/* ======================================================================
Function: valueAdd
Purpose : Add element to the Linked List of values
Input   : Pointer to the label name
          pointer to the value
          checksum value
          flag state of the label (modified by function)
Output  : pointer to the new node (or founded one)
Comments: - state of the label changed by the function
====================================================================== */
ValueList * TInfo::valueAdd(char * name, char * value, char * date, uint8_t checksum, uint8_t * flags)
{
  // Get our linked list 
  ValueList * me = &_valueslist;

  uint8_t lgname  = strlen(name);
  uint8_t lgvalue = strlen(value);
  uint8_t lgdate  = strlen(date);

  uint8_t thischeck = calcChecksum(name,value,date);

  // just some paranoia 
  if (thischeck != checksum ) {
    TI_Debug(name);
    TI_Debug('=');
    TI_Debug(value);
    TI_Debug(F(" '"));
    TI_Debug((char) checksum);
    TI_Debug(F("' Not added bad checksum calculated '"));
    TI_Debug((char) thischeck);
    TI_Debugln(F("'"));
  } else  {
    // Got one and all seems good ?
    if (me && lgname && lgvalue && checksum) {
      // Create pointer on the new node
      ValueList *newNode = NULL;
      ValueList *parNode = NULL ;

      // Loop thru the node
      while (me->next) {
        // save parent node
        parNode = me ;

        // go to next node
        me = me->next;

        // Check if we already have this LABEL (same name AND same size)
        if (lgname==strlen(me->name) && strcmp(me->name, name )==0) {

          // Already got also this value  return US
          if (lgvalue==strlen(me->value) && strcmp(me->value, value) == 0) {
            *flags |= TINFO_FLAGS_EXIST;
            me->flags = *flags;
            return ( me );
          } else {
            // We changed the value
            *flags |= TINFO_FLAGS_UPDATED;
            me->flags = *flags ;
            // Do we have enought space to hold new value ?    
            if (strlen(me->value) >= lgvalue ) {
              // Copy it
              strlcpy(me->value, value , lgvalue + 1);
              me->checksum = checksum ;

              // That's all
              return (me);
            } else {
              // indicate our parent node that the next node
              // is not us anymore but the next we have
              parNode->next = me->next;

              // free up this node
              free (me);

              // Return to parent (that will now point on next node and not us)
              // and continue loop just in case we have sevral with same name
              me = parNode;
            }
          }
        }
      }

      // Our linked list structure sizeof(ValueList)
      // + Name  + '\0'
      // + Value + '\0'
      size_t size ;
      #if defined (ESP8266) || defined (ESP32)
        lgname  = ESP_allocAlign(lgname+1);       // Align name buffer
        lgvalue = ESP_allocAlign(lgvalue+1);      // Align value buffer
        if (lgdate)
            lgdate  = ESP_allocAlign(lgdate+1);       // Align value buffer
            
        // Align the whole structure
        size = ESP_allocAlign( sizeof(ValueList) + lgname + lgvalue + lgdate +2)     ; 
      #else
        size = sizeof(ValueList) + lgname + 1 + lgvalue + 1 + lgdate + 1  ;
      #endif
      //Serial.print(" --- size malloc = ");
      //Serial.println(size);
      // Create new node with size to store strings
        if ((newNode = (ValueList  *) malloc(size) ) == NULL) {
            //--AD
            Serial.print(" ---  BUG size malloc = ");
            Serial.println(size);
            
            return ( (ValueList *) NULL );
        }
      // get our buffer Safe
      memset(newNode, 0, size);
      
      // Put the new node on the list
      me->next = newNode;

      // First String located after last struct element
      // Second String located after the First + \0
      newNode->checksum = checksum;
      newNode->name  = (char *) newNode + sizeof(ValueList);      
      newNode->value = (char *) newNode->name + lgname + 1;
      newNode->date  = (char *) newNode->value + lgvalue + 1;

      // Copy the string data
      memcpy(newNode->name , name  , lgname );
      memcpy(newNode->value, value , lgvalue );
      memcpy(newNode->date,  date  , lgdate );

      // So we just created this node but was it new
      // or was matter of text size ?
      if ( (*flags & TINFO_FLAGS_UPDATED) == 0) {
        // so we added this node !
        *flags |= TINFO_FLAGS_ADDED ;
        newNode->flags = *flags;
      }

      TI_Debug(F("Added '"));
      TI_Debug(name);
      TI_Debug('=');
      TI_Debug(value);
      TI_Debug('=');
      TI_Debug(date);
      TI_Debug(F("' '"));
      TI_Debug((char) checksum);
      TI_Debugln(F("'"));

      // return pointer on the new node
      return (newNode);
    }

  } // Checksum OK

  // Error or Already Exists
  return ( (ValueList *) NULL);
}



/* ======================================================================
Function: valueRemoveFlagged
Purpose : remove element to the Linked List of values where 
Input   : paramter flags
Output  : true if found and removed
Comments: -
====================================================================== */
boolean TInfo::valueRemoveFlagged(uint8_t flags)
{
  boolean deleted = false;

  // Get our linked list 
  ValueList * me = &_valueslist;
  ValueList *parNode = NULL ;

  // Got one and all seems good ?
  if (me) {
    // Loop thru the node
    while (me->next) {
      // save parent node
      parNode = me ;

      // go to next node
      me = me->next;

      // found the flags?
      if (me->flags & flags ) {
        // indicate our parent node that the next node
        // is not us anymore but the next we have
        parNode->next = me->next;

        // free up this node
        free (me);

        // Return to parent (that will now point on next node and not us)
        // and continue loop just in case we have sevral with same name
        me = parNode;
        deleted = true;
      }
    }
  }

  return (deleted);
}

/* ======================================================================
Function: valueRemove
Purpose : remove element to the Linked List of values
Input   : Pointer to the label name
Output  : true if found and removed
Comments: -
====================================================================== */
boolean TInfo::valueRemove(char * name)
{
  boolean deleted = false;

  // Get our linked list 
  ValueList * me = &_valueslist;
  ValueList *parNode = NULL ;

  uint8_t lgname = strlen(name);

  // Got one and all seems good ?
  if (me && lgname) {
    // Loop thru the node
    while (me->next) {
      // save parent node
      parNode = me ;

      // go to next node
      me = me->next;

      // found ?
      if (strncmp(me->name, name, lgname) == 0) {
        // indicate our parent node that the next node
        // is not us anymore but the next we have
        parNode->next = me->next;

        // free up this node
        free (me);

        // Return to parent (that will now point on next node and not us)
        // and continue loop just in case we have sevral with same name
        me = parNode;
        deleted = true;
      }
    }
  }

  return (deleted);
}

/* ======================================================================
Function: valueGet
Purpose : get value of one element
Input   : Pointer to the label name
          pointer to the value where we fill data 
Output  : pointer to the value where we filled data NULL is not found
====================================================================== */
char * TInfo::valueGet(char * name, char * value)
{
  // Get our linked list 
  ValueList * me = &_valueslist;
  uint8_t lgname = strlen(name);

  // Got one and all seems good ?
  if (me && lgname) {

    // Loop thru the node
    while (me->next) {

      // go to next node
      me = me->next;

      // Check if we match this LABEL
      if (lgname==strlen(me->name) && strcmp(me->name, name)==0) {
        // this one has a value ?
        if (me->value) {
          // copy to dest buffer
          uint8_t lgvalue = strlen(me->value);
          strlcpy(value, me->value , lgvalue + 1 );
          return ( value );
        }
      }
    }
  }
  // not found
  return ( NULL);
}

/* ======================================================================
Function: getTopList
Purpose : return a pointer on the top of the linked list
Input   : -
Output  : Pointer 
====================================================================== */
ValueList * TInfo::getList(void)
{
  // Get our linked list 
  return &_valueslist;
}

/* ======================================================================
Function: valuesDump
Purpose : dump linked list content
Input   : -
Output  : total number of values
====================================================================== */
uint8_t TInfo::valuesDump(void)
{
  // Get our linked list 
  ValueList * me = &_valueslist;
  uint8_t index = 0;

  // Got one ?
  if (me) {
    // Loop thru the node
    while (me->next) {
      // go to next node
      me = me->next;

      index++;
      TI_Debug(index) ;
      TI_Debug(F(") ")) ;

      if (me->name) {
        TI_Debug(me->name) ;
      } else {
        TI_Debug(F("NULL")) ;
      }

      TI_Debug(F("=")) ;

      if (me->value) {
        TI_Debug(me->value) ;
      } else {
        TI_Debug(F("NULL")) ;
      }

      TI_Debug(F(" '")) ;
      TI_Debug(me->checksum) ;
      TI_Debug(F("' ")); 

      // Flags management
      if ( me->flags) {
        TI_Debug(F("Flags:0x"));
          //---AD
        //TI_Debugf("%02X =>", me->flags);
        if ( me->flags & TINFO_FLAGS_EXIST) {
          TI_Debug(F("Exist ")) ;
        }
        if ( me->flags & TINFO_FLAGS_UPDATED) {
          TI_Debug(F("Updated ")) ;
        }
        if ( me->flags & TINFO_FLAGS_ADDED) {
          TI_Debug(F("New ")) ;
        }
      }

      TI_Debugln() ;
    }
  }

  return index;
}

/* ======================================================================
Function: labelCount
Purpose : Count the number of label in the list
Input   : -
Output  : element numbers
====================================================================== */
int TInfo::labelCount()
{
  int count = 0;

  // Get our linked list 
  ValueList * me = &_valueslist;

  if (me)
    while ((me = me->next))
      count++;

  return (count);
}

/* ======================================================================
Function: listDelete
Purpose : Delete the ENTIRE Linked List, not a value
Input   : -
Output  : True if Ok False Otherwise
====================================================================== */
boolean TInfo::listDelete()
{
  // Get our linked list 
  ValueList * me = &_valueslist;

  // Got a pointer
  if (me) {
    ValueList *current;

    // For each linked list
    while ((current = me->next)) {
      // Get the next
      me->next =  current->next;

      // Free the current
      free(current);
    }

    // Free the top element
    me->next = NULL ;

    // Ok
    return (true);
  }

  return (false);
}

/* ======================================================================
Function: checksum
Purpose : calculate the checksum based on data/value fields
Input   : label name 
          label value 
Output  : checksum
Comments: return '\0' in case of error
====================================================================== */
unsigned char TInfo::calcChecksum(char *etiquette, char *valeur, char *date) 
{
    //--AD
    bool fl = false;
    
    /*
    Serial.print(" Sum_lu: ");
    Serial.print(check3, HEX);
    Serial.print(" - ");
    */
       
    //uint8_t sum = ' ';  // Somme des codes ASCII du message + un espace
    uint8_t sum = 0;  // Somme des codes ASCII du message + 2 HT au mini

  // avoid dead loop, always check all is fine 
  if (etiquette && valeur) {
      //--AD
      //Serial.print(" -- etiq + valeur" );
      
    // this will not hurt and may save our life ;-)
    if (strlen(etiquette) && strlen(valeur)) {
        /*--AD
        Serial.print(" -- ln.etiq: " );
        Serial.print(strlen(etiquette));  Serial.print(" ");
        char *pp = etiquette;
        for(int i=0;i<strlen(etiquette);i++) {
            if (*pp < 17) Serial.print("0");
            Serial.print(*pp++,HEX);
        }
        Serial.println("");
        Serial.print(" -- ln.valeur: " ); Serial.print(" ");
        Serial.print(strlen(valeur));
        pp = valeur;
        for(int i=0;i<strlen(valeur);i++) {
            if (*pp < 17) Serial.print("0");
            Serial.print(*pp++,HEX);
        }
     */
      while (*etiquette)
        sum += *etiquette++ ;
      sum += TINFO_HT;

      if ( *valeur != '*') {
      //    fl = false;
          while(*valeur)
            sum += *valeur++ ;
      }
     // else fl = true;
      sum += TINFO_HT;

      if (strlen(date) > 2 ) {
          while(*date)
            sum += *date++ ;
          sum += TINFO_HT;
      }
      /*
      int Sum_cal =  (sum & 63) + ' ' ;
      if ( fl) {
          Serial.print(" Sum_lu: ");
          Serial.print(check3, HEX);
          Serial.print(" - ");
          Serial.print(" Sum_cal: ");
          Serial.println(Sum_cal, HEX);
      }
      */
      return ( (sum & 63) + ' ' ) ;
      //return Sum_cal ;
      //return (check3);
    }
  }
  return 0;
}

/* ======================================================================
Function: customLabel
Purpose : do action when received a correct label / value + checksum line
Input   : plabel : pointer to string containing the label
          pvalue : pointer to string containing the associated value
          pflags pointer in flags value if we need to cchange it
Output  : 
Comments: 
====================================================================== */
void TInfo::customLabel( char * plabel, char * pvalue, uint8_t * pflags) 
{
  int8_t phase = -1;

  // Monophasé
  if (strcmp(plabel, "ADPS")==0 ) 
    phase=0;

  // For testing
  //if (strcmp(plabel, "IINST")==0 ) {
  //  *pflags |= TINFO_FLAGS_ALERT;
  //}

  // triphasé c'est ADIR + Num Phase
  if (plabel[0]=='A' && plabel[1]=='D' && plabel[2]=='I' && plabel[3]=='R' && plabel[4]>='1' && plabel[4]<='3') {
    phase = plabel[4]-'0';
  }

  // Nous avons un ADPS ?
  if (phase>=0 && phase <=3) {
    // ne doit pas être sauvé définitivement
    *pflags |= TINFO_FLAGS_ALERT;
  
    // Traitement de l'ADPS demandé par le sketch
    if (_fn_ADPS) 
      _fn_ADPS(phase);
  }
}

/* ======================================================================
Function: checkLine
Purpose : check one line of teleinfo received
Input   : -
Output  : pointer to the data object in the linked list if OK else NULL
Comments: 
====================================================================== */
ValueList * TInfo::checkLine(char * pline) 
{
  char * p;
  char * ptok;
  char * pend;
  char * pvalue;
  char * phoro;
  char   checksum;
  char  buff[TINFO_BUFSIZE];
  uint8_t flags  = TINFO_FLAGS_NONE;
  //boolean err = true ;  // Assume  error
  int len ; // Group len
  char * p2;
  //int fl_h ;
  bool fl_h = false;
  char * pp;

  if (pline==NULL)
    return NULL;

  len = strlen(pline); 

  // a line should be at least 7 Char
  // 2 Label + Space + 1 etiquette + space + checksum + \r
  if ( len < 7 )
    return NULL;

  // Get our own working copy
  strlcpy( buff, pline, len+1);

  p = &buff[0];
  ptok = p;       // for sure we start with token name
  pend = p + len; // max size

  // Init values
  pvalue = NULL;
  phoro  = NULL;
  checksum = 0;

  TI_Debug("Got [");
  TI_Debug(len);
  TI_Debug("] ");
  
  // Loop in buffer 
  while ( p < pend ) {
    // start of token value
    //--AD if ( *p==' ' && ptok) {      // ------------ separation buffer mode Historique
    
    if ( *p==TINFO_HT && ptok) {        // ------------ separation buffer mode standart
        // Isolate token name
        *p++ = '\0';                    // couper le buffer pour isoler le label
      
        // 1st HT, it's the label value ou Horodatage
        if (!pvalue) {
            pvalue = p;
            if (!phoro) phoro = p; 
        }
        else {
            //Serial.print(" --- 2em ");
            // 2nd ou 3em HT, so it's the checksum ou le debut de la horo ou de la value .. 
            p2 = p + 1;                   // pointeur sur <CR>
            if ( *p2 == '\r' ) {  
                //Serial.print(" --- HT ");     
                checksum = *p;
                check3 = *p;
                ++p;                      // pointer sur <CR> pour passer à l'analyse.
            }           
            else {                        // on a un horodatage en plus 
                pvalue = p;               // debut de la value après l'horodatage
                --p;                      // revenir sur le HT du debut de value 
                //Serial.print(" --- Horo ");
                fl_h = true; 
                checksum = 0;
                if ( *++p2 == '\r' ) {    // test si champ vide = 2HT de suite
                    //Serial.println(" --- HT,HT ");
                    pvalue = &dummy[0];   // test  Value absente devient * 
                    ++p;     
                    checksum = *++p;
                    check3 = checksum;
                    ++p;              
                }
            } 
        }  
    }           
    // new line ? ok we got all we need ?
    
    if ( *p=='\r' ) {                     
      *p='\0';

      //--AD                                  // test Horodatage
      if (!fl_h) {
          phoro = &dummy[0];                  // mettre une etoile dans le champ Horodatatge           
      }
      fl_h = false;  
            
      //Serial.print(" --- Analyse, ");
   
      // Good format ?
      if ( ptok && pvalue && checksum ) {      
        //--AD
        //Serial.print(" --- format OK");
        // Always check to avoid bad behavior 
        if(strlen(ptok) && strlen(pvalue)) {
          //Serial.print(" --lg OK");
          // Is checksum is OK
          if ( calcChecksum(ptok,pvalue,phoro) == checksum) {
            //--AD
            //Serial.println(" trame OK"); 
            // In case we need to do things on specific labels
            customLabel(ptok, pvalue, &flags);

            // Add value to linked lists of values
            ValueList * me = valueAdd(ptok, pvalue, phoro, checksum, &flags);

            // value correctly added/changed
            if ( me ) {
              // something to do with new datas
              if (flags & (TINFO_FLAGS_UPDATED | TINFO_FLAGS_ADDED | TINFO_FLAGS_ALERT) ) {
                // this frame will for sure be updated
                _frame_updated = true;
                // Do we need to advertise user callback
                if (_fn_data)
                  _fn_data(me, flags);
              }
              /*else  Serial.print(" --- flag KO");*/
            }
           /* else   Serial.print(" --- add KO : ");*/
          }
          else {
            Serial.print(" --- trame KO : ");
            Serial.print(calcChecksum(ptok,pvalue,phoro)); Serial.print(" = ");
            Serial.println(checksum, HEX);
            Serial.print(" -- Label: ");
             pp = ptok;
             for(int i=0;i<=strlen(ptok);i++) {
                Serial.print(*pp++);
             }
             Serial.print(" -- Horo: ");
             pp = phoro;
             for(int i=0;i<=strlen(phoro);i++) {
                Serial.print(*pp++);
             }
             Serial.print(" -- Value: ");
             pp = pvalue;
             for(int i=0;i<=strlen(pvalue);i++) {
                Serial.print(*pp++);
             }
             Serial.println("");
          }
        }
        /* else  Serial.print(" --- lg KO");*/
      }
      /*  else Serial.print(" --- format KO"); */
    }           
    // Next char
    p++;

  } // While

  return NULL;
}

/* ======================================================================
Function: process
Purpose : teleinfo serial char received processing, should be called
          my main loop, this will take care of managing all the other
Input   : pointer to the serial used 
Output  : teleinfo global state
====================================================================== */
_State_e TInfo::process(char c)
{
   // be sure 7 bits only
   c &= 0x7F;
    //--AD
    //Serial.print(c, HEX);     / tout ce qui passe ...

  // What we received ?
  switch (c)  {
    // start of transmission ???
    case  TINFO_STX:
      // Clear buffer, begin to store in it
      clearBuffer();

      // by default frame is not "updated"
      // if data change we'll set this flag
      _frame_updated = false;

      // We were waiting fo this one ?
      if (_state == TINFO_INIT || _state == TINFO_WAIT_STX ) {
          TI_Debugln(F("TINFO_WAIT_ETX"));
         _state = TINFO_WAIT_ETX;
      } 
    break;
      
    // End of transmission ?
    case  TINFO_ETX:

      // Normal working mode ?
      if (_state == TINFO_READY) {
        // Get on top of our linked list 
        ValueList * me = &_valueslist;
        
        // Call user callback if any
        if (_frame_updated && _fn_updated_frame)
          _fn_updated_frame(me);
        else if (_fn_new_frame)
          _fn_new_frame(me);

        #ifdef TI_Debug
          valuesDump();
        #endif

        // It's important there since all user job is done
        // to remove the alert flags from table (ADPS for example)
        // it will be put back again next time if any
        valueRemoveFlagged(TINFO_FLAGS_ALERT);
      }

      // We were waiting fo this one ?
      if (_state == TINFO_WAIT_ETX) {
        TI_Debugln(F("TINFO_READY"));
        _state = TINFO_READY;
      } 
      else if ( _state == TINFO_INIT) {
        TI_Debugln(F("TINFO_WAIT_STX"));
        _state = TINFO_WAIT_STX ;
      } 

    break;

    // Start of group \n ?
    case  TINFO_SGR:
      // Do nothing we'll work at end of group
      // we can safely ignore this char
          //--AD
          //Serial.println("-Start of groupe-");
    break;

    // End of group \r ?
    case  TINFO_EGR:
          //--AD
          //Serial.println("\n-End of groupe-");  // ------------------ recu
      // Are we ready to process ?
      if (_state == TINFO_READY) {
        // Store data recceived (we'll need it)
        if ( _recv_idx < TINFO_BUFSIZE)
          _recv_buff[_recv_idx++]=c;

        // clear the end of buffer (paranoia inside)
        memset(&_recv_buff[_recv_idx], 0, TINFO_BUFSIZE-_recv_idx);

        // check the group we've just received
        checkLine(_recv_buff) ;

        // Whatever error or not, we done
        clearBuffer();
      }
    break;
    
    // other char ?
    default:
    {
        //--AD
        //Serial.print("--");
        //Serial.print(c);
        //Serial.println("");
        
      // Only in a ready state of course
      if (_state == TINFO_READY) {
        // If buffer is not full, Store data 
          if ( _recv_idx < TINFO_BUFSIZE) {
          _recv_buff[_recv_idx++]=c;
          //--AD
              //Serial.print(":");
              //Serial.print(c);
        }
        else
          clearBuffer();
      }
    }
    break;
  }

  return _state;
}
