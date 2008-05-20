//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, mC2 team
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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <core/tracklist/IRandomAccess.h>
#include <core/Query/ListBase.h>
#include <core/Query/TrackMetadata.h>
#include <core/Library/Base.h>

#include <set>
#include <sigslot/sigslot.h>
#include <boost/shared_ptr.hpp>
#include <boost/bimap.hpp>



//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{
    namespace tracklist {

        class Standard : public IRandomAccess, public sigslot::has_slots<> {

            public:
                
                typedef boost::shared_ptr<Standard> Ptr;

                Standard(void);
                ~Standard(void);

                musik::core::TrackPtr CurrentTrack();
                musik::core::TrackPtr NextTrack();
                musik::core::TrackPtr PreviousTrack();


                virtual void SetCurrentPosition(int position);
                virtual int CurrentPosition();

                virtual int Size();

                virtual musik::core::TrackPtr operator [](int position);
                virtual musik::core::TrackPtr TrackWithMetadata(int position);

                virtual void SetLibrary(musik::core::LibraryPtr setLibrary);
                virtual musik::core::LibraryPtr Library();

                virtual bool CopyTracks(musik::core::tracklist::IRandomAccess &tracklist);
                virtual bool AppendTracks(musik::core::tracklist::IRandomAccess &tracklist);

                virtual void AddRequestedMetakey(const char* metakey);
                virtual void RemoveRequestedMetakey(const char* metakey);

                virtual UINT64 Duration();
                virtual UINT64 Filesize();
                /////////////////////////////////////////////////////////////////////
                void ConnectToQuery(musik::core::Query::ListBase &listQuery);

                void HintNumberOfRows(int rows);

            private:

                void LoadTrack(int position);
                bool QueryForTrack(int position);
                void OnTracksFromQuery(musik::core::TrackVector *newTracks,bool clear);
                void OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks);
                void OnTracksInfoFromQuery(UINT64 tracks,UINT64 duration,UINT64 filesize);

                std::set<std::string> requestedMetaKeys;

                int currentPosition;
                int hintedRows;

                musik::core::LibraryPtr library;
                musik::core::TrackVector tracks;
                musik::core::Query::TrackMetadata trackQuery;

                typedef boost::bimap<int,musik::core::TrackPtr> TrackCache;
                TrackCache trackCache;

                int infoDuration;
                int infoFilesize;

//                bool InCache(int position);
//                bool InCache(musik::core::TrackPtr track);
        };
    }
} } // musik::core