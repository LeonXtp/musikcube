//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Daniel �nnerby
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include <core/Common.h>
#include <core/Library/Remote.h>
#include <core/Query/Base.h>
#include <core/Preferences.h>
#include <core/xml/Parser.h>
#include <core/xml/Writer.h>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

using namespace musik::core;


//////////////////////////////////////////
///\brief
///Constructor.
///
///The constructor will not start the Library.
///
///\see
///Startup
//////////////////////////////////////////
Library::Remote::Remote(utfstring identifier)
 :Base(identifier)
 ,socket(ioService)
{
}

//////////////////////////////////////////
///\brief
///Create a Remote library
//////////////////////////////////////////
LibraryPtr Library::Remote::Create(utfstring identifier){
	LibraryPtr lib(new Library::Remote(identifier));
	lib->self	= lib;
	return lib;
}

//////////////////////////////////////////
///\brief
///Destructor that exits and joins all threads
//////////////////////////////////////////
Library::Remote::~Remote(void){
    this->Exit();
    this->threads.join_all();
}

//////////////////////////////////////////
///\brief
///Get a short status string of what is going on in the Library.
///
///\returns
///Information string.
///
///The information is mostly used to get the information
///about the Indexer.
//////////////////////////////////////////
utfstring Library::Remote::GetInfo(){
    return UTF("");
}


//////////////////////////////////////////
///\brief
///Startup the library threads.
///
///\returns
///True if successfully started. This should always be true. Nothing else is expected.
//////////////////////////////////////////
bool Library::Remote::Startup(){

    // Lets start the ReadThread first, it will startup the connection and
    // then start the WriteThread
    this->threads.create_thread(boost::bind(&Library::Remote::ReadThread,this));

    return true;
}


//////////////////////////////////////////
///\brief 
///Thread for reading from the socket
//////////////////////////////////////////
void Library::Remote::ReadThread(){

    {
        Preferences prefs("Connection",this->Identifier().c_str());

        this->address   = ConvertUTF8(prefs.GetString("address",UTF("localhost")));
        this->port      = ConvertUTF8(prefs.GetString("port",UTF("10543")));
    }

    boost::asio::ip::tcp::resolver resolver(this->ioService);
    boost::asio::ip::tcp::resolver::query resolverQuery(this->address,this->port);

    try{
        boost::system::error_code resolverError;
        boost::asio::ip::tcp::resolver::iterator endpointIterator = resolver.resolve(resolverQuery,resolverError);
        boost::asio::ip::tcp::resolver::iterator end;

        if(resolverError){
            this->Exit();
            return;
        }

        boost::system::error_code error = boost::asio::error::host_not_found;
        while (error && endpointIterator!=end){
            this->socket.close();
            this->socket.connect(*endpointIterator, error);
            if(error){
                endpointIterator++;
            }
        }
        if (error || endpointIterator==end){
            this->Exit();
            return;
        }
    }
    catch(...){
        this->Exit();
        return;
    }

    // Successfully connected to server
    // Start the WriteThread
    try{
        this->threads.create_thread(boost::bind(&Library::Remote::WriteThread,this));
    }
    catch(...){
        this->Exit();
        return;
    }


    try{
        // Lets start recieving queries
        xml::Parser parser(&this->socket);
        if( xml::ParserNode rootNode=parser.ChildNode("musik")){
            while(xml::ParserNode node=rootNode.ChildNode()){
                if(node.Name()=="queryresults"){

                    unsigned int queryId    = boost::lexical_cast<unsigned int>(node.Attributes()["id"]);
                    unsigned int uniqueId   = boost::lexical_cast<unsigned int>(node.Attributes()["uid"]);
                    Query::Ptr currentQuery;
                    // This is a query node
                    // Find the query in the outgoingQueries list
                    {
                        boost::mutex::scoped_lock lock(this->libraryMutex);
                        // Reverse loop since it's most likely the query is in the end
                        for(QueryList::reverse_iterator query=this->outgoingQueries.rbegin();query!=this->outgoingQueries.rend();++query){
                            if( (*query)->uniqueId==uniqueId ){
                                currentQuery    = *query;
                            }
                        }
                    }
                    if(currentQuery){
                        if(currentQuery->RecieveResults(node,this)){
                            boost::mutex::scoped_lock lock(this->libraryMutex);
                            currentQuery->status |= Query::Base::Status::Ended;
                        }else{
                            boost::mutex::scoped_lock lock(this->libraryMutex);
                            currentQuery->status |= Query::Base::Status::Canceled | Query::Base::Status::Ended;
                        }

                        this->waitCondition.notify_all();

                    }
                }
            }
        }
    }
    catch(...){

    }
    this->Exit();
}


//////////////////////////////////////////
///\brief
///Thread for writing to the socket
//////////////////////////////////////////
void Library::Remote::WriteThread(){

    xml::Writer writer(&this->socket);

    // Start by writing the musik-tag
    xml::WriterNode rootNode(writer,"musik");
    
    while(!this->Exited()){
        Query::Ptr query(this->GetNextQuery());

        if(query){    // No empty query

            ////////////////////////////////////////////////////////////
            // Add to the finished queries
            {
                boost::mutex::scoped_lock lock(this->libraryMutex);
                this->bCurrentQueryCanceled    = false;
                this->runningQuery    = query;
                this->outgoingQueries.push_back(query);

                // Set query as started
                query->status |= Query::Base::Status::Started;
            }

            ////////////////////////////////////////////////////////////
            // Lets send the query
            xml::WriterNode queryNode(rootNode,"query");
            queryNode.Attributes()["type"]  = query->Name();
            queryNode.Attributes()["id"]    = boost::lexical_cast<std::string>(query->queryId);
            queryNode.Attributes()["uid"]   = boost::lexical_cast<std::string>(query->uniqueId);

            if(query->options){
                queryNode.Attributes()["options"]  = boost::lexical_cast<std::string>(query->options);
            }

			if(!query->SendQuery(queryNode)){
				// Query can not be send, lets cancel it
                boost::mutex::scoped_lock lock(this->libraryMutex);
				query->status |= Query::Base::Status::Canceled | Query::Base::Status::Ended;
			}

            ////////////////////////////////////////////////////////////
            // Notify that the Query is finished.
            this->waitCondition.notify_all();

            // Check if writer has quit
            if(writer.Exited()){
                this->Exit();
            }

        }else{

            ////////////////////////////////////////////////////////////
            // Tricky part, waiting for queries to be added.
            // Not sure I'm doing this the right way.
            // Could this part lead to a deadlock???
            boost::mutex::scoped_lock lock(this->libraryMutex);
            if(!this->exit && this->incomingQueries.size()==0 ){
                this->waitCondition.wait(lock);
            }
        }
    }

}

//////////////////////////////////////////
///\brief
///Cancel the current running query
///
///This method will also send a sqlite3_interrupt to cancel the
///current running SQL Query
//////////////////////////////////////////
void Library::Remote::CancelCurrentQuery( ){
    this->bCurrentQueryCanceled    = true;
}


//////////////////////////////////////////
///\brief
///Exit the library
///
///Will set the library to Exited, close sockets and notify all sleeping threads
//////////////////////////////////////////
void Library::Remote::Exit(){
    {
        boost::mutex::scoped_lock lock(this->libraryMutex);
        if(!this->exit){
            if(this->socket.is_open()){
                this->socket.close();
            }
        }
        this->exit    = true;
    }
    this->waitCondition.notify_all();
}

//////////////////////////////////////////
///\brief
///Get the base path to where the tracks are located
///
///This method is mostly used by the Library::Remote to
///get the HTTP-address to the tracks
//////////////////////////////////////////
utfstring Library::Remote::BasePath(){
    utfstring path(UTF("http://"));
    boost::asio::ip::tcp::endpoint endPoint = this->socket.remote_endpoint();
    boost::asio::ip::address address        = endPoint.address();

    path    += musik::core::ConvertUTF16(address.to_string());
    path    += UTF(":") + musik::core::ConvertUTF16(this->port) + UTF("/");
    return path;
}
