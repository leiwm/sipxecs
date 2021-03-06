//
// Copyright (C) 2008 Pingtel Corp., certain elements licensed under a Contributor Agreement.
// Contributors retain copyright to elements licensed under a Contributor Agreement.
// Licensed to the User under the LGPL license.
//
//////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include "utl/UtlBool.h"
#include "utl/UtlHashBag.h"
#include "utl/UtlHashBagIterator.h"
#include "utl/UtlHashMapIterator.h"
#include "utl/UtlSListIterator.h"
#include "os/OsSysLog.h"
#include "os/OsFileSystem.h"
#include "net/XmlRpcDispatch.h"
#include "net/XmlRpcMethod.h"
#include "net/XmlRpcRequest.h"

#include "sipdb/ResultSet.h"
#include "sipdb/SIPDBManager.h"
#include "sipdb/AliasDB.h"
#include "sipdb/CallerAliasDB.h"
#include "sipdb/PermissionDB.h"
#include "sipdb/CredentialDB.h"
#include "sipdb/RegistrationDB.h"
#include "sipdb/ExtensionDB.h"
#include "sipdb/UserLocationDB.h"
#include "sipdb/UserForwardDB.h"
#include "sipdb/UserStaticDB.h"

#include "SipxRpc.h"
#include "ImdbRpc.h"
#include "ImdbResource.h"
#include "ImdbResourceManager.h"

// DEFINES
#define  URI            "uri"
#define  CONTACT        "contact"
#define  REALM          "realm"
#define  USERID         "userid"
#define  PASSTOKEN      "passtoken"
#define  PINTOKEN       "pintoken"
#define  AUTHTYPE       "authtype"
#define  IDENTITY       "identity"
#define  USER           "user"
#define  DOMAIN         "domain"

#define  CREDENTIAL     "credential"
#define  ALIAS          "alias"
#define  CALLER_ALIAS   "caller-alias"
#define  PERMISSION     "permission"
#define  EXTENSION      "extension"
#define  REGISTRATION   "registration"
#define  USERLOCATION   "userlocation"
#define  USERFORWARD    "userforward"
#define  USERSTATIC     "userstatic"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/*************************************************************************
 **** ImdbRpcMethod contains common code for ImdbRpc methods
 ************************************************************************/
const char* ImdbRpcMethod::METHOD_NAME = "ImdbRpc.BASE";
const char* ImdbRpcMethod::PARAM_NAME_CALLING_HOST = "callingHostname";
const char* ImdbRpcMethod::PARAM_NAME_IMDB_TABLE = "IMDBTableName";
const char* ImdbRpcMethod::PARAM_NAME_IMDB_TABLE_DATA = "IMDBTableRecords";
const char* ImdbRpcMethod::PARAM_NAME_IMDB_STATUS = "status";
const char* ImdbRpcMethod::PARAM_NAME_IMDB_SESSION_NAME = "sessionId";

XmlRpcMethod* ImdbRpcMethod::get()
{
   assert(false);  // this should have been overridden in the subclass

   return NULL;
}

void ImdbRpcMethod::registerSelf(SipxRpc & sipxRpcImpl)
{
   assert(false);  // this should have been overridden in the subclass
}

ImdbRpcMethod::ImdbRpcMethod()
{
}

void ImdbRpcMethod::registerMethod(const char*       methodName,
                                   XmlRpcMethod::Get getMethod,
                                   SipxRpc &         sipxRpcImpl
                                   )
{
   sipxRpcImpl.getXmlRpcDispatch()->addMethod(methodName, getMethod, &sipxRpcImpl );
}

bool ImdbRpcMethod::execute(const HttpRequestContext& requestContext,
                                UtlSList& params,
                                void* userData,
                                XmlRpcResponse& response,
                                ExecutionStatus& status
                                )
{
   assert(false); // this should have been overridden in the subclass

   return false;
}

bool ImdbRpcMethod::validCaller(const HttpRequestContext& requestContext,
                                const UtlString&          peerName,
                                XmlRpcResponse&           response,
                                const SipxRpc&            sipxRpcImpl,
                                const char*               callingMethod
                                )
{
   bool result = false;

   if (!peerName.isNull() && requestContext.isTrustedPeer(peerName))
   {
      // ssl says the connection is from the named host
      if (sipxRpcImpl.isAllowedPeer(peerName))
      {
         // sipXsupervisor says it is one of the allowed peers.
         result = true;
         OsSysLog::add(FAC_SUPERVISOR, PRI_DEBUG,
                       "ImdbMethod::validCaller '%s' peer authenticated for %s",
                       peerName.data(), callingMethod
                       );
      }
      else
      {
         // this peer is authenticated, but not configured, so provide a good error response
         UtlString faultMsg;
         faultMsg.append("Unconfigured calling host '");
         faultMsg.append(peerName);
         faultMsg.append("'");
         response.setFault(ImdbRpcMethod::UnconfiguredPeer, faultMsg.data());

         OsSysLog::add(FAC_SUPERVISOR, PRI_ERR,
                       "%s failed - '%s' not a configured peer",
                       callingMethod, peerName.data()
                       );
      }
   }
   else
   {
      // ssl says not authenticated - provide only a generic error
      response.setFault(XmlRpcResponse::AuthenticationRequired, "TLS Peer Authentication Failure");

      OsSysLog::add(FAC_SUPERVISOR, PRI_ERR,
                    "%s failed: '%s' failed SSL authentication",
                    callingMethod, peerName.data()
                    );
   }

   return result;
}

void ImdbRpcMethod::handleMissingExecuteParam(const char* methodName,
                                                  const char* paramName,
                                                  XmlRpcResponse& response,
                                                  ExecutionStatus& status)
{
   UtlString faultMsg;
   faultMsg += methodName;
   faultMsg += " '";
   faultMsg += paramName;
   faultMsg += "' parameter is missing or invalid type";
   status = XmlRpcMethod::FAILED;
   response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
   OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, faultMsg);
}

void ImdbRpcMethod::handleExtraExecuteParam(const char* methodName,
                                                XmlRpcResponse& response,
                                                ExecutionStatus& status)
{
   UtlString faultMsg;
   faultMsg += methodName;
   faultMsg += " has incorrect number of parameters";
   status = XmlRpcMethod::FAILED;
   response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
   OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, faultMsg);
}


/*write the database's content to xml file
*/
void ImdbRpcMethod::storeTable(const UtlString& tableName){

    if ( tableName == CREDENTIAL ){
        if ( CredentialDB::getInstance()->store() != OS_SUCCESS ) {
        }
    }

    else if ( tableName == ALIAS ){
        if ( AliasDB::getInstance()->store() != OS_SUCCESS ) {
        }
    }

    else if ( tableName == PERMISSION ){
        if ( PermissionDB::getInstance()->store() != OS_SUCCESS ){
        }
    }

    else if ( tableName == EXTENSION ){
        if ( ExtensionDB::getInstance()->store() != OS_SUCCESS ){
        }
    }

    else if ( tableName == CALLER_ALIAS ){
       if ( CallerAliasDB::getInstance()->store() != OS_SUCCESS ){
       }
    }

    else if ( tableName == USERLOCATION ){
       if ( UserLocationDB::getInstance()->store() != OS_SUCCESS ){
       }
    }

    else if ( tableName == USERFORWARD ){
       if ( UserForwardDB::getInstance()->store() != OS_SUCCESS ){
       }
    }

    else if ( tableName == USERSTATIC ){
       if ( UserStaticDB::getInstance()->store() != OS_SUCCESS ){
       }
    }
}


UtlBoolean ImdbRpcMethod::insertTableRecord(UtlString& tableName, const UtlHashMap& tableRecord)
{
    UtlBoolean   result;

    static UtlString identityKey      (IDENTITY);
    static UtlString domainKey        (DOMAIN);
    static UtlString aliasKey         (ALIAS);

    result = FALSE;

    if ( tableName == CREDENTIAL ){
        result = CredentialDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == ALIAS ){
        result = AliasDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == PERMISSION ){
        result = PermissionDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == EXTENSION ){
       result = ExtensionDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == USERLOCATION ){
       result = UserLocationDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == USERFORWARD ){
       result = UserForwardDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == USERSTATIC ){
       result = UserStaticDB::getInstance()->insertRow(tableRecord);
    }

    else if ( tableName == CALLER_ALIAS ){
       UtlString* optionalIdentity = dynamic_cast<UtlString*>(tableRecord.findValue(&identityKey));
       UtlString nullIdentity;

       result = TRUE;
       CallerAliasDB::getInstance()->
          insertRow(optionalIdentity ? *optionalIdentity : nullIdentity,
                    *dynamic_cast<UtlString*>(tableRecord.findValue(&domainKey)),
                    *dynamic_cast<UtlString*>(tableRecord.findValue(&aliasKey))
                    );
    }

    return result;
}



/*****************************************************************
 **** ImdbRpcReplaceTable
 *****************************************************************/

const char* ImdbRpcReplaceTable::METHOD_NAME = "ImdbTable.replace";

const char* ImdbRpcReplaceTable::name()
{
   return METHOD_NAME;
}

ImdbRpcReplaceTable::ImdbRpcReplaceTable()
{
}

XmlRpcMethod* ImdbRpcReplaceTable::get()
{
   return new ImdbRpcReplaceTable();
}

void ImdbRpcReplaceTable::registerSelf(SipxRpc & sipxRpcImpl)
{
   registerMethod(METHOD_NAME, ImdbRpcReplaceTable::get, sipxRpcImpl);
}

void ImdbRpcReplaceTable::insertPartialRecord(const UtlString& tableName, UtlHashMap* pTableRecord, UtlSList* pTableVector)
{
  //
  //  Do a really crappy copying of objects from the hashmap.
  //  We do not have any choice.  UtlStuff is not a zero-copy implementation
  //  STL containers anyone?
  //

  if ( tableName == CREDENTIAL )
  {

    UtlString* pUriKey = new UtlString("uri");
    UtlString* pRealmKey = new UtlString("realm");
    UtlString* pUserIdKey = new UtlString("userid");
    UtlString* pPassTokenKey = new UtlString("passtoken");
    UtlString* pPinTokenKey = new UtlString("pintoken");
    UtlString* pAuthTypeKey = new UtlString("authtype");

    UtlString* pUri = new UtlString();
    UtlString* pRealm = new UtlString();
    UtlString* pUserId = new UtlString();
    UtlString* pPassToken = new UtlString();
    UtlString* pPinToken = new UtlString();
    UtlString* pAuthType = new UtlString();

    *pUri = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pUriKey)));
    *pRealm = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pRealmKey)));
    *pUserId = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pUserIdKey)));
    *pPassToken = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pPassTokenKey)));
    *pPinToken = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pPinTokenKey)));
    *pAuthType = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pAuthTypeKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pUriKey, pUri);
    pClonedRecord->insertKeyAndValue(pRealmKey, pRealm);
    pClonedRecord->insertKeyAndValue(pUserIdKey, pUserId);
    pClonedRecord->insertKeyAndValue(pPassTokenKey, pPassToken);
    pClonedRecord->insertKeyAndValue(pPinTokenKey, pPinToken);
    pClonedRecord->insertKeyAndValue(pAuthTypeKey, pAuthType);

    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == ALIAS )
  {
    UtlString* pIdentityKey = new UtlString("identity");
    UtlString* pContactKey = new UtlString("contact");
    UtlString* pRelationKey = new UtlString("relation");

    UtlString* pIdentity = new UtlString();
    UtlString* pContact = new UtlString();
    UtlString* pRelation = new UtlString();

    *pIdentity = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pIdentityKey)));
    *pContact = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pContactKey)));
    // For upward compatibility from the old alias format which has no
    // <relation> value, if the relation value is not provided, use
    // the null string:
    UtlString* relationp = dynamic_cast <UtlString*> (pTableRecord->findValue(pRelationKey));
    if (relationp)
    {
       *pRelation = *relationp;
    }

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pIdentityKey, pIdentity);
    pClonedRecord->insertKeyAndValue(pContactKey, pContact);
    pClonedRecord->insertKeyAndValue(pRelationKey, pRelation);
    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == PERMISSION )
  {
    UtlString* pIdentityKey = new UtlString("identity");
    UtlString* pPermissionKey = new UtlString("permission");

    UtlString* pIdentity = new UtlString();
    UtlString* pPermission = new UtlString();

    *pIdentity = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pIdentityKey)));
    *pPermission = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pPermissionKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pIdentityKey, pIdentity);
    pClonedRecord->insertKeyAndValue(pPermissionKey, pPermission);
    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == EXTENSION )
  {
    UtlString* pUriKey = new UtlString("uri");
    UtlString* pExtensionKey = new UtlString("extension");

    UtlString* pUri = new UtlString();
    UtlString* pExtension = new UtlString();

    *pUri =  *(dynamic_cast <UtlString*> (pTableRecord->findValue(pUriKey))),
    *pExtension = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pExtensionKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pUriKey, pUri);
    pClonedRecord->insertKeyAndValue(pExtensionKey, pExtension);
    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == USERLOCATION )
  {
    UtlString* pIdentityKey = new UtlString("identity");
    UtlString* pLocationKey = new UtlString("location");

    UtlString* pIdentity = new UtlString();
    UtlString* pLocation = new UtlString();

    *pIdentity = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pIdentityKey)));
    *pLocation = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pLocationKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pIdentityKey, pIdentity);
    pClonedRecord->insertKeyAndValue(pLocationKey, pLocation);
    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == USERFORWARD )
  {
    UtlString* pIdentityKey = new UtlString("identity");
    UtlString* pCfwdtimeKey = new UtlString("cfwdtime");

    UtlString* pIdentity = new UtlString();
    UtlString* pCfwdtime = new UtlString();

    *pIdentity = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pIdentityKey)));
    *pCfwdtime = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pCfwdtimeKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pIdentityKey, pIdentity);
    pClonedRecord->insertKeyAndValue(pCfwdtimeKey, pCfwdtime);
    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == USERSTATIC )
  {
    UtlString* pIdentityKey = new UtlString("identity");
    UtlString* pEventKey = new UtlString("event");
    UtlString* pContactKey = new UtlString("contact");
    UtlString* pFromUriKey = new UtlString("from_uri");
    UtlString* pToUriKey = new UtlString("to_uri");
    UtlString* pCallidKey = new UtlString("callid");

    UtlString* pIdentity = new UtlString();
    UtlString* pEvent = new UtlString();
    UtlString* pContact = new UtlString();
    UtlString* pFromUri = new UtlString();
    UtlString* pToUri = new UtlString();
    UtlString* pCallid = new UtlString();

    *pIdentity = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pIdentityKey)));
    *pEvent = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pEventKey)));
    *pContact = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pContactKey)));
    *pFromUri = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pFromUriKey)));
    *pToUri = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pToUriKey)));
    *pCallid = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pCallidKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pIdentityKey, pIdentity);
    pClonedRecord->insertKeyAndValue(pEventKey, pEvent);
    pClonedRecord->insertKeyAndValue(pContactKey, pContact);
    pClonedRecord->insertKeyAndValue(pFromUriKey, pFromUri);
    pClonedRecord->insertKeyAndValue(pToUriKey, pToUri);
    pClonedRecord->insertKeyAndValue(pCallidKey, pCallid);
    pTableVector->append(pClonedRecord);
  }
  else if ( tableName == CALLER_ALIAS )
  {
    UtlString* pIdentityKey = new UtlString("identity");
    UtlString* pDomainKey = new UtlString("domain");
    UtlString* pAliasKey = new UtlString("alias");

    UtlString* pIdentity = new UtlString();
    UtlString* pDomain = new UtlString();
    UtlString* pAlias = new UtlString();

    *pIdentity = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pIdentityKey)));
    *pDomain = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pDomainKey)));
    *pAlias = *(dynamic_cast <UtlString*> (pTableRecord->findValue(pAliasKey)));

    UtlHashMap* pClonedRecord = new UtlHashMap();
    pClonedRecord->insertKeyAndValue(pIdentityKey, pIdentity);
    pClonedRecord->insertKeyAndValue(pDomainKey, pDomain);
    pClonedRecord->insertKeyAndValue(pAliasKey, pAlias);
    pTableVector->append(pClonedRecord);
  }
}

bool ImdbRpcReplaceTable::executePartialReplace(const HttpRequestContext& requestContext,
                                     UtlSList& params,
                                     void* userData,
                                     XmlRpcResponse& response,
                                     ExecutionStatus& status)
{
  static UtlString credentialSessionId;
  static UtlString aliasSessionId;
  static UtlString permissionSessionId;
  static UtlString extensionSessionId;
  static UtlString userLocationSessionId;
  static UtlString userForwardSessionId;
  static UtlString userStaticSessionId;
  static UtlString callerAliasSessionId;



  if (!params.at(0) || !params.at(0)->isInstanceOf(UtlString::TYPE))
  {
    handleMissingExecuteParam(name(), PARAM_NAME_CALLING_HOST, response, status);
    return false;
  }

  if (!params.at(1) || !params.at(1)->isInstanceOf(UtlString::TYPE))
  {
    handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE, response, status);
    return false;
  }

  if (!params.at(2) || !params.at(2)->isInstanceOf(UtlSList::TYPE))
  {
    handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA, response, status);
    return false;
  }

  if (!params.at(3) || !params.at(3)->isInstanceOf(UtlString::TYPE))
  {
     handleMissingExecuteParam(name(), PARAM_NAME_IMDB_STATUS, response, status);
     return false;
  }


  if (!params.at(4) || !params.at(4)->isInstanceOf(UtlString::TYPE))
  {
     handleMissingExecuteParam(name(), PARAM_NAME_IMDB_SESSION_NAME, response, status);
     return false;;
  }


  UtlString* pCallingHostname = dynamic_cast<UtlString*>(params.at(0));
  UtlString* pIMDBTable = dynamic_cast<UtlString*>(params.at(1));
  UtlSList* pIMDBTableData = dynamic_cast<UtlSList*>(params.at(2));
  UtlString* pIMDBStatus = dynamic_cast<UtlString*>(params.at(3));
  UtlString* pIMDBSessionName = dynamic_cast<UtlString*>(params.at(4));



  SipxRpc* pSipxRpcImpl = ((SipxRpc *)userData);
 if(!validCaller(requestContext, *pCallingHostname, response, *pSipxRpcImpl, name()))
   return false;

  ImdbResource* imdbResource = ImdbResourceManager::getInstance()->find(pIMDBTable->data());
 if (!imdbResource || !imdbResource->isWriteable())
 {
   UtlString faultMsg;
   faultMsg.append("IMDB table '");
                     faultMsg.append(*pIMDBTable);
                     faultMsg.append("' is not a resource OR is not writable (configAccess='read-only')");
   OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpc::replaceFile %s",
                 faultMsg.data());
   response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
   return false;
 }


  OsSysLog::add(FAC_SUPERVISOR, PRI_DEBUG, "ImdbRpcReplaceTable::executePartialReplace queuing %d records for resource %s",
                 (int)pIMDBTableData->entries(), pIMDBTable->data());


  bool result = false;

  bool isFinal = ((*pIMDBStatus) == "final");

  UtlString* pActiveSessionName = 0;

  if ( *pIMDBTable == CREDENTIAL )
  {
    pActiveSessionName = &credentialSessionId;
  }
  else if ( *pIMDBTable == ALIAS )
  {
    pActiveSessionName = &aliasSessionId;
  }
  else if ( *pIMDBTable == PERMISSION )
  {
    pActiveSessionName = &permissionSessionId;
  }
  else if ( *pIMDBTable == EXTENSION )
  {
    pActiveSessionName = &extensionSessionId;
  }
  else if ( *pIMDBTable == USERLOCATION )
  {
    pActiveSessionName = &userLocationSessionId;
  }
  else if ( *pIMDBTable == USERFORWARD )
  {
    pActiveSessionName = &userForwardSessionId;
  }
  else if ( *pIMDBTable == USERSTATIC )
  {
    pActiveSessionName = &userStaticSessionId;
  }
  else if ( *pIMDBTable == CALLER_ALIAS )
  {
    pActiveSessionName = &callerAliasSessionId;
  }
  else
  {
    UtlString faultMsg;
    faultMsg.append("IMDB table '");
                     faultMsg.append(*pIMDBTable);
                     faultMsg.append("' is not a resource");
    OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpc::replaceFile %s",
                 faultMsg.data());
    response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
    return false;
  }



  if ((*pActiveSessionName) != (*pIMDBSessionName))
  {
    //
    // This is a new session.  clear the content of the active vector
    //
    // we've validated the caller and the parm types.
    // steps are to clear the existing table
    // and then insert each record individually.
    clearTable( *pIMDBTable );
    //
    // Remember the new session id
    //
    (*pActiveSessionName) = (*pIMDBSessionName);
  }

  UtlBool method_result(true);
  OsSysLog::add(FAC_SUPERVISOR, PRI_DEBUG, "ImdbRpcReplaceTable::executePartialReplace persisting %d records for resource %s",
                 (int)pIMDBTableData->entries(), pIMDBTable->data());

   UtlSListIterator tableRecordItor(*pIMDBTableData);
   UtlHashMap*      tableRecord;
   UtlBoolean insert_result;
   while ( (tableRecord = dynamic_cast <UtlHashMap*> (tableRecordItor())))
   {
      // records are committed as soon as they're added.
      insert_result = insertTableRecord(*pIMDBTable, *tableRecord);
      if (   ( FALSE == insert_result )
          && ( true == method_result.getValue()))
      {
         method_result = false; // @TODO this isn't very satisfactory.
      }
   }

  if (isFinal)
  {

     if ( method_result.getValue() == true )
     {
       OsSysLog::add(FAC_SUPERVISOR, PRI_DEBUG, "ImdbRpcReplaceTable::executePartialReplace finalized persistence for resource %s",
                  pIMDBTable->data());

        storeTable( *pIMDBTable );
        imdbResource->modified();
     }
     result = true;
  }
  else
  {
    //
    // This is a partial recordset
    //
    result = true;
  }

   if (!result)
   {
     UtlString faultMsg;
     faultMsg.append("IMDB table '");
                       faultMsg.append(*pIMDBTable);
                       faultMsg.append("' - Unknown error occured while writing to IMDB");
     OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpc::replaceFile %s",
                   faultMsg.data());
     response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
   }
   else
   {
     // Construct and set the response.
     response.setResponse(&method_result);
     status = XmlRpcMethod::OK;
   }

  return result;
}

bool ImdbRpcReplaceTable::execute(const HttpRequestContext& requestContext,
                                     UtlSList& params,
                                     void* userData,
                                     XmlRpcResponse& response,
                                     ExecutionStatus& status)
{
   if (5 == params.entries())
     return executePartialReplace(requestContext, params, userData, response, status);

   bool result = false;
   status = XmlRpcMethod::FAILED;
   UtlString faultMsg;

   // Verify that the number of parameters expected is correct.
   if (3 != params.entries())
   {
      handleExtraExecuteParam(name(), response, status);
   }
   else
   {
      // Extract the paramaters and verify them.  This method expects
      // the calling host name, IMDB Table name and the IMDB Table data records.
      if (!params.at(0) || !params.at(0)->isInstanceOf(UtlString::TYPE))
      {
         handleMissingExecuteParam(name(), PARAM_NAME_CALLING_HOST, response, status);
      }
      else
      {
         UtlString* pCallingHostname = dynamic_cast<UtlString*>(params.at(0));

         SipxRpc* pSipxRpcImpl = ((SipxRpc *)userData);
         if(validCaller(requestContext, *pCallingHostname, response, *pSipxRpcImpl, name()))
         {
            if (!params.at(1) || !params.at(1)->isInstanceOf(UtlString::TYPE))
            {
               handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE, response, status);
            }
            else
            {
               UtlString* pIMDBTable = dynamic_cast<UtlString*>(params.at(1));

               // find the ImdbResource object for this table
               ImdbResource* imdbResource;
               if ((imdbResource = ImdbResourceManager::getInstance()->find(pIMDBTable->data())))
               {
                  if (imdbResource->isWriteable())
                  {
                     if (!params.at(2) || !params.at(2)->isInstanceOf(UtlSList::TYPE))
                     {
                        handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA,
                                                  response, status);
                     }
                     else
                     {
                        UtlSList* pIMDBTableData = dynamic_cast<UtlSList*>(params.at(2));

                        // Verify that at least the first row of table data is of the correct type.
                        if (   !pIMDBTableData->isEmpty() /* an empty table is allowed */
                            && !pIMDBTableData->at(0)->isInstanceOf(UtlHashMap::TYPE)
                            )
                        {
                           // the table data list was non-empty but the first item was not a UtlHashMap
                           handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA,
                                                     response, status);
                        }
                        else
                        {
                           // we've validated the caller and the parm types.
                           // steps are to clear the existing table
                           // and then insert each record individually.
                           clearTable( *pIMDBTable );

                           UtlSListIterator tableRecordItor(*pIMDBTableData);
                           UtlHashMap*      tableRecord;
                           UtlBool method_result(true);
                           UtlBoolean insert_result;
                           while ( (tableRecord = dynamic_cast <UtlHashMap*> (tableRecordItor())))
                           {
                              // records are committed as soon as they're added.
                              insert_result = insertTableRecord(*pIMDBTable, *tableRecord);
                              if (   ( FALSE == insert_result )
                                  && ( true == method_result.getValue()))
                              {
                                 method_result = false; // @TODO this isn't very satisfactory.
                              }
                           }

                           if ( method_result.getValue() == true )
                           {
                              storeTable( *pIMDBTable );

                              imdbResource->modified();
                           }

                           // Construct and set the response.
                           response.setResponse(&method_result);
                           status = XmlRpcMethod::OK;
                           result = true;
                        }
                     }
                  }
                  else
                  {
                     faultMsg.append("IMDB table '");
                     faultMsg.append(*pIMDBTable);
                     faultMsg.append("' is not writable (configAccess='read-only')");
                     OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpc::replaceFile %s",
                                   faultMsg.data());
                     result = false;
                     response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
                  }
               }
               else
               {
                  faultMsg.append("IMDB table '");
                  faultMsg.append(*pIMDBTable);
                  faultMsg.append("' is not declared as a resource by any sipXecs process");
                  OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpc::replaceFile %s",
                                faultMsg.data());
                  result = false;
                  response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
               }
            }
         }
      }
   }

   return result;
}

void ImdbRpcReplaceTable::clearTable(UtlString& tableName)
{
    if ( tableName == CREDENTIAL )
    {
        CredentialDB::getInstance()->removeAllRows();
    }
    else if ( tableName == ALIAS )
    {
        AliasDB::getInstance()->removeAllRows();
    }
    else if ( tableName == PERMISSION )
    {
        PermissionDB::getInstance()->removeAllRows();
    }
    else if ( tableName == EXTENSION )
    {
       ExtensionDB::getInstance()->removeAllRows();
    }
    else if ( tableName == CALLER_ALIAS )
    {
       CallerAliasDB::getInstance()->removeAllRows();
    }
    else if ( tableName == USERLOCATION )
    {
       UserLocationDB::getInstance()->removeAllRows();
    }
    else if ( tableName == USERFORWARD )
    {
       UserForwardDB::getInstance()->removeAllRows();
    }
    else if ( tableName == USERSTATIC )
    {
       UserStaticDB::getInstance()->removeAllRows();
    }
    else
    {
       OsSysLog::add(FAC_SUPERVISOR, PRI_CRIT, "ImdbRpcReplaceTable::clearTable "
                     "invalid table name '%s'", tableName.data());
    }
}


/*****************************************************************
 **** ImdbRpcRetrieveTable
 *****************************************************************/

const char* ImdbRpcRetrieveTable::METHOD_NAME = "ImdbTable.read";

const char* ImdbRpcRetrieveTable::name()
{
   return METHOD_NAME;
}

ImdbRpcRetrieveTable::ImdbRpcRetrieveTable()
{
}

XmlRpcMethod* ImdbRpcRetrieveTable::get()
{
   return new ImdbRpcRetrieveTable();
}

void ImdbRpcRetrieveTable::registerSelf(SipxRpc & sipxRpcImpl)
{
   registerMethod(METHOD_NAME, ImdbRpcRetrieveTable::get, sipxRpcImpl);
}

bool ImdbRpcRetrieveTable::execute(const HttpRequestContext& requestContext,
                                     UtlSList& params,
                                     void* userData,
                                     XmlRpcResponse& response,
                                     ExecutionStatus& status)
{
   bool result = false;
   status = XmlRpcMethod::FAILED;

   // Verify that the number of parameters expected is correct.
   if (2 != params.entries())
   {
      handleExtraExecuteParam(name(), response, status);
   }
   else
   {
      // Extract the paramaters and verify them.  This method expects
      // the calling host name and IMDB Table name.
      if (!params.at(0) || !params.at(0)->isInstanceOf(UtlString::TYPE))
      {
         handleMissingExecuteParam(name(), PARAM_NAME_CALLING_HOST, response, status);
      }
      else
      {
         UtlString* pCallingHostname = dynamic_cast<UtlString*>(params.at(0));

         SipxRpc* pSipxRpcImpl = ((SipxRpc *)userData);

         if(validCaller(requestContext, *pCallingHostname, response, *pSipxRpcImpl, name()))
         {
            if (!params.at(1) || !params.at(1)->isInstanceOf(UtlString::TYPE))
            {
               handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE, response, status);
            }
            else
            {
               UtlString faultMsg;

               UtlString* pIMDBTable = dynamic_cast<UtlString*>(params.at(1));

               ImdbResource* imdbResource;
               if ((imdbResource = ImdbResourceManager::getInstance()->find(pIMDBTable->data())))
               {

                  OsSysLog::add(FAC_SUPERVISOR, PRI_INFO,
                                "ImdbRpc::retrieveTable"
                                " '%s' read table '%s'",
                                pCallingHostname->data(), pIMDBTable->data()
                                );

                  // Get the records from the appropriate IMDB table.
                  ResultSet   imdb_tabledata;

                  readTable( *pIMDBTable, &imdb_tabledata ); // (This dynamically allocates memory.)

                  // Construct and set the response.
                  response.setResponse(&imdb_tabledata);
                  status = XmlRpcMethod::OK;
                  result = true;

                  // Delete the new'd objects
                  imdb_tabledata.destroyAll();
               }
               else
               {
                  faultMsg.append("IMDB table '");
                  faultMsg.append(*pIMDBTable);
                  faultMsg.append("' is not declared as a resource by any sipXecs process");
                  OsSysLog::add(FAC_SUPERVISOR, PRI_INFO, "ImdbRpc::retrieveTable %s",
                                faultMsg.data());
                  result = false;
                  response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
               }
            }
         }
      }
   }

   return result;
}

void ImdbRpcRetrieveTable::readTable(UtlString& tableName, ResultSet* imdb_records)
{
    if ( tableName == CREDENTIAL ){
        CredentialDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == REGISTRATION ){
        RegistrationDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == ALIAS ){
        AliasDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == PERMISSION ){
        PermissionDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == EXTENSION ){
       ExtensionDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == USERLOCATION ){
       UserLocationDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == USERFORWARD ){
       UserForwardDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == USERSTATIC ){
       UserStaticDB::getInstance()->getAllRows( *imdb_records );
    }

    else if ( tableName == CALLER_ALIAS ){
       // CallerAliasDB::getInstance()->getAllRows( *imdb_records );
    }

}



/*****************************************************************
 **** ImdbRpcAddTableRecords
 *****************************************************************/

const char* ImdbRpcAddTableRecords::METHOD_NAME = "ImdbTable.insertRows";

const char* ImdbRpcAddTableRecords::name()
{
   return METHOD_NAME;
}

ImdbRpcAddTableRecords::ImdbRpcAddTableRecords()
{
}

XmlRpcMethod* ImdbRpcAddTableRecords::get()
{
   return new ImdbRpcAddTableRecords();
}

void ImdbRpcAddTableRecords::registerSelf(SipxRpc & sipxRpcImpl)
{
   registerMethod(METHOD_NAME, ImdbRpcAddTableRecords::get, sipxRpcImpl);
}

bool ImdbRpcAddTableRecords::execute(const HttpRequestContext& requestContext,
                                     UtlSList& params,
                                     void* userData,
                                     XmlRpcResponse& response,
                                     ExecutionStatus& status)
{
   bool result = false;
   status = XmlRpcMethod::FAILED;

   // Verify that the number of parameters expected is correct.
   if (3 != params.entries())
   {
      handleExtraExecuteParam(name(), response, status);
   }
   else
   {
      // Extract the paramaters and verify them.  This method expects
      // the calling host name, IMDB Table name and the IMDB Table data records.
      if (!params.at(0) || !params.at(0)->isInstanceOf(UtlString::TYPE))
      {
         handleMissingExecuteParam(name(), PARAM_NAME_CALLING_HOST, response, status);
      }
      else
      {
         UtlString* pCallingHostname = dynamic_cast<UtlString*>(params.at(0));

         SipxRpc* pSipxRpcImpl = ((SipxRpc *)userData);
         if(validCaller(requestContext, *pCallingHostname, response, *pSipxRpcImpl, name()))
         {
            if (!params.at(1) || !params.at(1)->isInstanceOf(UtlString::TYPE))
            {
               handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE, response, status);
            }
            else
            {
               UtlString* pIMDBTable = dynamic_cast<UtlString*>(params.at(1));

               UtlString faultMsg;

               // find the ImdbResource object for this table
               ImdbResource* imdbResource;
               if ((imdbResource = ImdbResourceManager::getInstance()->find(pIMDBTable->data())))
               {
                  if (imdbResource->isWriteable())
                  {
                     if (!params.at(2) || !params.at(2)->isInstanceOf(UtlSList::TYPE))
                     {
                        handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA,
                                                  response, status);
                     }
                     else
                     {
                        UtlSList* pIMDBTableData = dynamic_cast<UtlSList*>(params.at(2));

                        // Verify that the table data is of the correct type.
                        if (   !pIMDBTableData->at(0)
                            || !pIMDBTableData->at(0)->isInstanceOf(UtlHashMap::TYPE))
                        {
                           handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA,
                                                     response, status);
                        }
                        else
                        {
                           // we've validated the parm types and the caller.

                           UtlSListIterator tableRecordItor(*pIMDBTableData);
                           UtlHashMap*      tableRecord;
                           UtlBool method_result(true);
                           UtlBoolean insert_result;
                           while ( (tableRecord = dynamic_cast <UtlHashMap*> (tableRecordItor())))
                           {
                              // records are committed as soon as they're added.
                              insert_result = insertTableRecord(*pIMDBTable, *tableRecord);
                              if (   ( FALSE == insert_result )
                                  && ( true == method_result.getValue() ) )
                              {
                                 method_result = false; // @TODO not very satisfactory
                              }
                           }

                           // If we successfully added the new rows,
                           // write out the table to an XML file.
                           if ( method_result.getValue() == true )
                           {
                              storeTable( *pIMDBTable );

                              imdbResource->modified();
                           }

                           // Construct and set the response.
                           response.setResponse(&method_result);
                           status = XmlRpcMethod::OK;
                           result = true;
                        }
                     }
                  }
                  else
                  {
                     faultMsg.append("IMDB table '");
                     faultMsg.append(*pIMDBTable);
                     faultMsg.append("' is not writable (configAccess='read-only')");
                     OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpcAddTableRecords::execute %s",
                                   faultMsg.data());
                     result = false;
                     response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
                  }
               }
               else
               {
                  faultMsg.append("IMDB table '");
                  faultMsg.append(*pIMDBTable);
                  faultMsg.append("' is not declared as a resource by any sipXecs process");
                  OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpcAddTableRecords::execute %s",
                                faultMsg.data());
                  result = false;
                  response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
               }
            }
         }
      }
   }

   return result;
}


/*****************************************************************
 **** ImdbRpcDeleteTableRecords
 *****************************************************************/

const char* ImdbRpcDeleteTableRecords::METHOD_NAME = "ImdbTable.deleteRows";

const char* ImdbRpcDeleteTableRecords::name()
{
   return METHOD_NAME;
}

ImdbRpcDeleteTableRecords::ImdbRpcDeleteTableRecords()
{
}

XmlRpcMethod* ImdbRpcDeleteTableRecords::get()
{
   return new ImdbRpcDeleteTableRecords();
}

void ImdbRpcDeleteTableRecords::registerSelf(SipxRpc & sipxRpcImpl)
{
   registerMethod(METHOD_NAME, ImdbRpcDeleteTableRecords::get, sipxRpcImpl);
}

bool ImdbRpcDeleteTableRecords::execute(const HttpRequestContext& requestContext,
                                     UtlSList& params,
                                     void* userData,
                                     XmlRpcResponse& response,
                                     ExecutionStatus& status)
{
   bool result = false;
   status = XmlRpcMethod::FAILED;

   // Verify that the number of parameters expected is correct.
   if (3 != params.entries())
   {
      handleExtraExecuteParam(name(), response, status);
   }
   else
   {
      // Extract the paramaters and verify them.  This method expects
      // the calling host name, IMDB Table name and the IMDB Table data record keys.
      if (!params.at(0) || !params.at(0)->isInstanceOf(UtlString::TYPE))
      {
         handleMissingExecuteParam(name(), PARAM_NAME_CALLING_HOST, response, status);
      }
      else
      {
         UtlString* pCallingHostname = dynamic_cast<UtlString*>(params.at(0));

         SipxRpc* pSipxRpcImpl = ((SipxRpc *)userData);

         if(validCaller(requestContext, *pCallingHostname, response, *pSipxRpcImpl, name()))
         {
            if (!params.at(1) || !params.at(1)->isInstanceOf(UtlString::TYPE))
            {
               handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE, response, status);
            }
            else
            {
               UtlString* pIMDBTable = dynamic_cast<UtlString*>(params.at(1));

               // find the ImdbResource object for this table
               UtlString faultMsg;
               ImdbResource* imdbResource;
               if ((imdbResource = ImdbResourceManager::getInstance()->find(pIMDBTable->data())))
               {
                  if (imdbResource->isWriteable())
                  {

                     if (!params.at(2) || !params.at(2)->isInstanceOf(UtlSList::TYPE))
                     {
                        handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA,
                                                  response, status);
                     }
                     else
                     {
                        UtlSList* pIMDBTableDataKeys = dynamic_cast<UtlSList*>(params.at(2));

                        // Verify that the table data is of the correct type.
                        if (   !pIMDBTableDataKeys->at(0)
                            || !pIMDBTableDataKeys->at(0)->isInstanceOf(UtlHashMap::TYPE))
                        {
                           handleMissingExecuteParam(name(), PARAM_NAME_IMDB_TABLE_DATA,
                                                     response, status);
                        }

                        // we've validated the parm types and the caller.

                        UtlSListIterator tableRecordKeysItor(*pIMDBTableDataKeys);
                        UtlHashMap*      tableRecordKeys;
                        UtlBool method_result(true);
                        UtlBoolean delete_result;
                        while ((tableRecordKeys=dynamic_cast<UtlHashMap*>(tableRecordKeysItor())))
                        {
                           // record deletion is immediate.
                           delete_result = deleteTableRecord(*pIMDBTable, *tableRecordKeys);
                           if (   ( FALSE == delete_result )
                               && ( true == method_result.getValue()))
                           {
                              method_result = false;
                           }
                        }

                        // If we successfully deleted the rows, write out the table to an XML file.
                        if ( method_result.getValue() == true )
                        {
                           storeTable( *pIMDBTable );
                        }

                        // Construct and set the response.
                        response.setResponse(&method_result);
                        status = XmlRpcMethod::OK;
                        result = true;
                     }
                  }
                  else
                  {
                     faultMsg.append("IMDB table '");
                     faultMsg.append(*pIMDBTable);
                     faultMsg.append("' is not writable (configAccess='read-only')");
                     OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpcDeleteTableRecords::execute %s",
                                   faultMsg.data());
                     result = false;
                     response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
                  }
               }
               else
               {
                  faultMsg.append("IMDB table '");
                  faultMsg.append(*pIMDBTable);
                  faultMsg.append("' is not declared as a resource by any sipXecs process");
                  OsSysLog::add(FAC_SUPERVISOR, PRI_ERR, "ImdbRpcDeleteTableRecords::execute %s",
                                faultMsg.data());
                  result = false;
                  response.setFault(ImdbRpcMethod::InvalidParameter, faultMsg);
               }
            }
         }
      }
   }

   return result;
}

UtlBoolean ImdbRpcDeleteTableRecords::deleteTableRecord(UtlString& tableName, const UtlHashMap& tableRecordKeys)
{
    UtlBoolean   result;

    static UtlString uriKey           (URI);
    static UtlString extensionKey     (EXTENSION);
    static UtlString contactKey       (CONTACT);
    static UtlString realmKey         (REALM);
    static UtlString useridKey        (USERID);
    static UtlString passtokenKey     (PASSTOKEN);
    static UtlString pintokenKey      (PINTOKEN);
    static UtlString authtypeKey      (AUTHTYPE);
    static UtlString identityKey      (IDENTITY);
    static UtlString permissionKey    (PERMISSION);
    static UtlString userKey          (USER);
    static UtlString domainKey        (DOMAIN);
    static UtlString aliasKey         (ALIAS);

    result = FALSE;

    if ( tableName == CREDENTIAL ){
        CredentialDB::getInstance()->removeRows(
                     Url (*((UtlString*)tableRecordKeys.findValue(&uriKey))) );
        result = TRUE;
    }

    else if ( tableName == ALIAS ){
        result = AliasDB::getInstance()->removeRow(
                     Url (*((UtlString*)tableRecordKeys.findValue(&identityKey))) );
    }

    else if ( tableName == PERMISSION ){
        PermissionDB::getInstance()->removeRows(
                     Url (*((UtlString*)tableRecordKeys.findValue(&identityKey))) );
        result = TRUE;
    }

    else if ( tableName == EXTENSION ){
        result = ExtensionDB::getInstance()->removeRow(
                     Url (*((UtlString*)tableRecordKeys.findValue(&uriKey))) );
    }

    else if ( tableName == USERLOCATION ){
          UserLocationDB::getInstance()->removeRows(
                     Url (*((UtlString*)tableRecordKeys.findValue(&identityKey))) );
       result = TRUE;
    }

    else if ( tableName == USERFORWARD ){
          UserForwardDB::getInstance()->removeRow(
                     Url (*((UtlString*)tableRecordKeys.findValue(&identityKey))) );
       result = TRUE;
    }

    else if ( tableName == USERSTATIC ){
          UserStaticDB::getInstance()->removeRow(
                     Url (*((UtlString*)tableRecordKeys.findValue(&identityKey))) );
       result = TRUE;
    }

    return result;
}

