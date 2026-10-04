// sv_demo_writer.cpp -- the files of the server demos, written by a thread of
// their own
//
// Creating a file or writing to it can block for tens of milliseconds on a
// busy disk, and a server frame that waits holds up every player.  So the
// frame only fills memory: each file collects what is written to it and
// hands it to the writer thread in blocks.  The thread does every disk access
// of the demos (directories, open, write, close, remove) in the order asked.
// It calls nothing of the engine but Sys_Mkdir: no file system handles, no
// Com_Printf, no zone memory.  A failure is a flag the frame reads.

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "server.h"

// a file hands over what it collected at this size, or after this long
#define	DEMOFILE_BLOCK		16384
#define	DEMOFILE_DELAY		1000

// a disk that can't keep up: past this much data waiting for the thread, the
// writes fail (and the demos stop) rather than fill the memory
#define	DEMOFILE_QUEUE_MAX	( 64 * 1024 * 1024 )

struct svDemoFile_s {
	// the server frame
	char				qpath[MAX_QPATH];
	std::vector<byte>	block;			// not handed over yet
	int					blockStart;		// Sys_Milliseconds of its first byte

	// set before the thread gets the file, then read by it
	char				osPath[MAX_OSPATH];

	// the thread
	FILE				*f;

	// both
	std::atomic<bool>	failed;			// open, write or close
	std::atomic<bool>	done;			// closed: the thread doesn't touch it any more

	svDemoFile_s() : blockStart( 0 ), f( NULL ), failed( false ), done( false ) {
		qpath[0] = osPath[0] = '\0';
	}
};

enum demoJobType_t {
	DJ_OPEN,
	DJ_WRITE,
	DJ_CLOSE,
	DJ_REMOVE		// close and delete
};

struct demoJob_t {
	demoJobType_t		type;
	svDemoFile_t		*file;
	std::vector<byte>	data;
};

// on the heap and never destroyed by the exit code: a thread still running
// when the process ends (a crash) finds it intact
struct demoWriter_t {
	std::mutex				mutex;
	std::condition_variable	wake;
	std::deque<demoJob_t>	jobs;
	size_t					queued;		// bytes in jobs
	bool					quit;
	std::thread				*thread;

	demoWriter_t() : queued( 0 ), quit( false ), thread( NULL ) {}
};

static demoWriter_t					*sv_demoWriter;

// the server frame: every file the thread isn't done with
static std::vector<svDemoFile_t *>	sv_demoFiles;

/*
=============================================================================

THE THREAD

=============================================================================
*/

// FS_CreatePath without its checks (done in SV_DemoFileOpen) or messages
static void SV_DemoWriterCreatePath( const char *osPath ) {
	char	path[MAX_OSPATH];
	char	*ofs;

	Q_strncpyz( path, osPath, sizeof( path ) );
	for ( ofs = path + 1; *ofs; ofs++ ) {
		if ( *ofs == PATH_SEP ) {
			*ofs = '\0';
			Sys_Mkdir( path );
			*ofs = PATH_SEP;
		}
	}
}

static void SV_DemoWriterRun( demoJob_t &job ) {
	svDemoFile_t	*file = job.file;

	switch ( job.type ) {
	case DJ_OPEN:
		SV_DemoWriterCreatePath( file->osPath );
		file->f = fopen( file->osPath, "wb" );
		if ( !file->f ) {
			file->failed = true;
		}
		break;

	case DJ_WRITE:
		if ( file->f && !job.data.empty() && fwrite( &job.data[0], 1, job.data.size(), file->f ) != job.data.size() ) {
			file->failed = true;
		}
		break;

	case DJ_CLOSE:
	case DJ_REMOVE:
		if ( file->f ) {
			if ( fclose( file->f ) ) {
				file->failed = true;
			}
			file->f = NULL;
			if ( job.type == DJ_REMOVE ) {
				remove( file->osPath );
			}
		}
		// the last access: the server frame can delete it from now on
		file->done = true;
		break;
	}
}

static void SV_DemoWriterThread( demoWriter_t *writer ) {
	std::unique_lock<std::mutex>	lock( writer->mutex );

	for ( ;; ) {
		while ( writer->jobs.empty() && !writer->quit ) {
			writer->wake.wait( lock );
		}
		if ( writer->jobs.empty() ) {
			return;		// quit with everything written
		}

		// swapped rather than moved: MSVC 2013 makes no move constructors
		demoJob_t	job;
		demoJob_t	&next = writer->jobs.front();

		job.type = next.type;
		job.file = next.file;
		job.data.swap( next.data );
		writer->jobs.pop_front();
		lock.unlock();

		SV_DemoWriterRun( job );

		lock.lock();
		writer->queued -= job.data.size();
	}
}

/*
=============================================================================

THE SERVER FRAME

=============================================================================
*/

// data, emptied, goes to the thread with the job
static void SV_DemoWriterPush( demoJobType_t type, svDemoFile_t *file, std::vector<byte> *data ) {
	const size_t	size = data ? data->size() : 0;

	if ( !sv_demoWriter ) {
		sv_demoWriter = new demoWriter_t;
		sv_demoWriter->thread = new std::thread( SV_DemoWriterThread, sv_demoWriter );
	}

	{
		std::lock_guard<std::mutex>	lock( sv_demoWriter->mutex );

		if ( type == DJ_WRITE && sv_demoWriter->queued + size > DEMOFILE_QUEUE_MAX ) {
			file->failed = true;
			data->clear();
			return;
		}

		sv_demoWriter->jobs.push_back( demoJob_t() );
		demoJob_t &job = sv_demoWriter->jobs.back();
		job.type = type;
		job.file = file;
		if ( data ) {
			job.data.swap( *data );
		}
		sv_demoWriter->queued += size;
	}
	sv_demoWriter->wake.notify_one();
}

// deletes the files the thread is done with
static void SV_DemoFilesPrune( void ) {
	size_t	i;

	for ( i = 0; i < sv_demoFiles.size(); ) {
		if ( sv_demoFiles[i]->done ) {
			delete sv_demoFiles[i];
			sv_demoFiles[i] = sv_demoFiles.back();
			sv_demoFiles.pop_back();
		} else {
			i++;
		}
	}
}

/*
===============
SV_DemoFileOpen

Creates the file <fs_homepath>/<game>/<qpath>, emptied if it exists; NULL for
a path FS_CreatePath refuses.  The thread opens it: a failure shows later in
SV_DemoFileFailed.
===============
*/
svDemoFile_t *SV_DemoFileOpen( const char *qpath ) {
	const char		*osPath = FS_BuildOSPath( Cvar_VariableString( "fs_homepath" ), "", qpath );
	svDemoFile_t	*file;

	// FS_CreatePath doesn't let a path go back up
	if ( strstr( osPath, ".." ) || strstr( osPath, "::" ) ) {
		return NULL;
	}

	file = new svDemoFile_t;
	Q_strncpyz( file->qpath, qpath, sizeof( file->qpath ) );
	Q_strncpyz( file->osPath, osPath, sizeof( file->osPath ) );
	sv_demoFiles.push_back( file );

	SV_DemoWriterPush( DJ_OPEN, file, NULL );
	return file;
}

void SV_DemoFileWrite( svDemoFile_t *file, const void *data, int len ) {
	const byte	*p = (const byte *)data;
	const int	now = Sys_Milliseconds();

	if ( file->block.empty() ) {
		file->blockStart = now;
	}
	file->block.insert( file->block.end(), p, p + len );

	if ( file->block.size() >= DEMOFILE_BLOCK || now - file->blockStart >= DEMOFILE_DELAY ) {
		SV_DemoWriterPush( DJ_WRITE, file, &file->block );
	}
}

qboolean SV_DemoFileFailed( const svDemoFile_t *file ) {
	return (qboolean)file->failed.load();
}

/*
===============
SV_DemoFileClose

What was written goes to the disk and the file is closed, or deleted with
discard.  file is not to be used any more.
===============
*/
void SV_DemoFileClose( svDemoFile_t *file, qboolean discard ) {
	if ( !discard && !file->block.empty() ) {
		SV_DemoWriterPush( DJ_WRITE, file, &file->block );
	}
	SV_DemoWriterPush( discard ? DJ_REMOVE : DJ_CLOSE, file, NULL );
}

/*
===============
SV_DemoFileInUse

A demo file open, or closed but not written to the end yet: FS_FileExists
may not see it yet
===============
*/
qboolean SV_DemoFileInUse( const char *qpath ) {
	size_t	i;

	SV_DemoFilesPrune();
	for ( i = 0; i < sv_demoFiles.size(); i++ ) {
		if ( !Q_stricmp( sv_demoFiles[i]->qpath, qpath ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
===============
SV_DemoFilesShutdown

Waits for the thread to write and close everything (the files must have
been closed), then stops it
===============
*/
void SV_DemoFilesShutdown( void ) {
	if ( sv_demoWriter ) {
		{
			std::lock_guard<std::mutex>	lock( sv_demoWriter->mutex );
			sv_demoWriter->quit = true;
		}
		sv_demoWriter->wake.notify_one();
		sv_demoWriter->thread->join();

		delete sv_demoWriter->thread;
		delete sv_demoWriter;
		sv_demoWriter = NULL;
	}
	SV_DemoFilesPrune();
}
