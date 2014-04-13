#include "../Interface/Thread.h"
#include "../Interface/Database.h"
#include "../Interface/Query.h"
#include "../Interface/SettingsReader.h"
#include "../Interface/Mutex.h"
#include "../Interface/ThreadPool.h"
#include "../urlplugin/IUrlFactory.h"
#include "fileclient/FileClient.h"
#include "fileclient/FileClientChunked.h"
#include "../urbackupcommon/os_functions.h"
#include "server_hash.h"
#include "server_prepare_hash.h"
#include "server_status.h"
#include "../urbackupcommon/sha2/sha2.h"
#include "../urbackupcommon/fileclient/tcpstack.h"
#include "server_settings.h"

class ServerVHDWriter;
class IFile;
class IPipe;
class ServerPingThread;
class FileClient;
class IPipeThrottler;

struct SBackup
{
	int incremental;
	std::wstring path;
	int incremental_ref;
	std::wstring complete;
	bool is_complete;
	bool is_resumed;
};

class BackupServerGet : public IThread, public FileClientChunked::ReconnectionCallback,
	public FileClient::ReconnectionCallback, public INotEnoughSpaceCallback,
	public FileClient::NoFreeSpaceCallback, public FileClientChunked::NoFreeSpaceCallback
{
public:
	BackupServerGet(IPipe *pPipe, sockaddr_in pAddr, const std::wstring &pName, bool internet_connection, bool use_snapshots, bool use_reflink);
	~BackupServerGet(void);

	void operator()(void);

	bool sendClientMessage(const std::string &msg, const std::string &retok, const std::wstring &errmsg, unsigned int timeout, bool logerr=true, int max_loglevel=LL_ERROR, bool *retok_err=NULL, std::string* retok_str=NULL);
	bool sendClientMessageRetry(const std::string &msg, const std::string &retok, const std::wstring &errmsg, unsigned int timeout, size_t retry=0, bool logerr=true, int max_loglevel=LL_ERROR, bool *retok_err=NULL, std::string* retok_str=NULL);
	std::string sendClientMessage(const std::string &msg, const std::wstring &errmsg, unsigned int timeout, bool logerr=true, int max_loglevel=LL_ERROR);
	std::string sendClientMessageRetry(const std::string &msg, const std::wstring &errmsg, unsigned int timeout, size_t retry=0, bool logerr=true, int max_loglevel=LL_ERROR);
	void sendToPipe(const std::string &msg);
	int getPCDone(void);

	sockaddr_in getClientaddr(void);

	static void init_mutex(void);
	static void destroy_mutex(void);

	static bool isInBackupWindow(std::vector<STimeSpan> bw);
	static MailServer getMailServerSettings(void);
	static bool sendMailToAdmins(const std::string& subj, const std::string& message);

	static int getNumberOfRunningBackups(void);
	static int getNumberOfRunningFileBackups(void);
	static int getClientID(IDatabase *db, const std::wstring &clientname, ServerSettings *server_settings, bool *new_client);

	IPipe *getClientCommandConnection(int timeoutms=10000, std::string* clientaddr=NULL);

	virtual IPipe * new_fileclient_connection(void);

	virtual bool handle_not_enough_space(const std::wstring &path);

private:
	void unloadSQL(void);
	void prepareSQL(void);
	void updateLastseen(void);
	bool isUpdateFull(void);
	bool isUpdateIncr(void);
	bool isUpdateFullImage(void);
	bool isUpdateIncrImage(void);
	bool isUpdateFullImage(const std::string &letter);
	bool isUpdateIncrImage(const std::string &letter);
	bool doFullBackup(bool with_hashes, bool &disk_error, bool &log_backup);
	int createBackupSQL(int incremental, int clientid, std::wstring path, bool resumed);
	void hashFile(std::wstring dstpath, std::wstring hashpath, IFile *fd, IFile *hashoutput, std::string old_file);
	void start_shadowcopy(const std::string &path);
	void stop_shadowcopy(const std::string &path);
	void notifyClientBackupSuccessfull(void);
	bool request_filelist_construct(bool full, bool resume, bool with_token, bool& no_backup_dirs, bool& connect_fail);
	bool load_file(const std::wstring &fn, const std::wstring &short_fn, const std::wstring &curr_path, const std::wstring &os_path, FileClient &fc, bool with_hashes, const std::wstring &last_backuppath, const std::wstring &last_backuppath_complete, bool &download_ok, bool hashed_transfer, bool save_incomplete_file);
	bool link_file(const std::wstring &fn, const std::wstring &short_fn, const std::wstring &curr_path, const std::wstring &os_path, bool with_hashes, const std::string& sha2, _i64 filesize, bool add_sql);
	bool load_file_patch(const std::wstring &fn, const std::wstring &short_fn, const std::wstring &curr_path, const std::wstring &os_path, const std::wstring &last_backuppath, const std::wstring &last_backuppath_complete, FileClientChunked &fc, FileClient &fc_normal, bool save_incomplete_file, bool &download_ok);
	bool doIncrBackup(bool with_hashes, bool intra_file_diffs, bool on_snapshot, bool use_directory_links, bool &disk_error, bool &log_backup, bool& r_incremental, bool& r_resumed);
	SBackup getLastIncremental(void);
	bool hasChange(size_t line, const std::vector<size_t> &diffs);
	void updateLastBackup(void);
	void updateLastImageBackup(void);
	void sendClientBackupIncrIntervall(void);
	void sendSettings(void);
	bool getClientSettings(bool& doesnt_exist);
	bool updateClientSetting(const std::wstring &key, const std::wstring &value);
	void setBackupComplete(void);
	void setBackupDone(void);
	void setBackupImageComplete(void);
	void sendClientLogdata(void);
	std::wstring getUserRights(int userid, std::string domain);
	void saveClientLogdata(int image, int incremental, bool r_success, bool resumed);
	void sendLogdataMail(bool r_success, int image, int incremental, int errors, int warnings, int infos, std::wstring &data);
	bool doImage(const std::string &pLetter, const std::wstring &pParentvhd, int incremental, int incremental_ref, bool transfer_checksum, bool compress);
	std::string getMBR(const std::wstring &dl);
	unsigned int writeMBR(ServerVHDWriter *vhdfile, uint64 volsize);
	int createBackupImageSQL(int incremental, int incremental_ref, int clientid, std::wstring path, std::string letter);
	SBackup getLastIncrementalImage(const std::string &letter);
	void updateRunning(bool image);
	void checkClientVersion(void);
	bool sendFile(IPipe *cc, IFile *f, int timeout);
	bool isBackupsRunningOkay(bool incr, bool file);
	void startBackupRunning(bool file);
	void stopBackupRunning(bool file);

	void sendBackupOkay(bool b_okay);
	void waitForFileThreads(void);

	bool deleteFilesInSnapshot(const std::string clientlist_fn, const std::vector<size_t> &deleted_ids, std::wstring snapshot_path, bool no_error);

	std::wstring fixFilenameForOS(const std::wstring& fn);

	_u32 getClientFilesrvConnection(FileClient *fc, int timeoutms=10000);
	FileClientChunked getClientChunkedFilesrvConnection(int timeoutms=10000);

	void saveImageAssociation(int image_id, int assoc_id);
	
	std::wstring constructImagePath(const std::wstring &letter, bool compress);
	bool constructBackupPath(bool with_hashes, bool on_snapshot, bool create_fs);
	void resetEntryState(void);
	bool getNextEntry(char ch, SFile &data, std::map<std::wstring, std::wstring>* extra);
	static std::string remLeadingZeros(std::string t);
	bool updateCapabilities(void);


	_i64 getIncrementalSize(IFile *f, const std::vector<size_t> &diffs, bool all=false);

	int64 updateNextblock(int64 nextblock, int64 currblock, sha256_ctx *shactx, unsigned char *zeroblockdata, bool parent_fn, ServerVHDWriter *parentfile, IFile *hashfile, IFile *parenthashfile, unsigned int blocksize, int64 mbr_offset, int64 vhd_blocksize, bool &warned_about_parenthashfile_error);

	std::wstring convertToOSPathFromFileClient(std::wstring path);
	IFile *getTemporaryFileRetry(void);
	void destroyTemporaryFile(IFile *tmp);

	IPipeThrottler *getThrottler(size_t speed_bps);

	void update_sql_intervals(bool update_sql);

	bool verify_file_backup(IFile *fileentries);

	std::string getSHA256(const std::wstring& fn);

	void logVssLogdata(void);

	bool createDirectoryForClient(void);

	void createHashThreads(bool use_reflink);
	void destroyHashThreads();

	void copyFile(const std::wstring& source, const std::wstring& dest);

	bool exponentialBackoff(size_t count, int64 lasttime, unsigned int sleeptime, unsigned div);
	bool exponentialBackoffImage();
	bool exponentialBackoffFile();

	bool authenticatePubKey();

	SSettings curr_intervals;

	IPipe *pipe;
	IDatabase *db;

	sockaddr_in clientaddr;
	IMutex *clientaddr_mutex;
	std::wstring clientname;
	
	std::wstring backuppath;
	std::wstring dir_pool_path;
	std::wstring backuppath_hashes;
	std::wstring backuppath_single;

	std::wstring tmpfile_path;
	static size_t tmpfile_num;
	static IMutex *tmpfile_mutex;

	int clientid;
	int backupid;

	ISettingsReader *settings;
	ISettingsReader *settings_client;
	ServerSettings *server_settings;

	IQuery *q_update_lastseen;
	IQuery *q_update_full;
	IQuery *q_update_incr;
	IQuery *q_create_backup;
	IQuery *q_get_last_incremental;
	IQuery *q_set_last_backup;
	IQuery *q_update_setting;
	IQuery *q_insert_setting;
	IQuery *q_set_complete;
	IQuery *q_update_image_full;
	IQuery *q_update_image_incr;
	IQuery *q_create_backup_image;
	IQuery *q_set_image_complete;
	IQuery *q_set_last_image_backup;
	IQuery *q_get_last_incremental_image;
	IQuery *q_set_image_size;
	IQuery *q_update_running_file;
	IQuery *q_update_running_image;
	IQuery *q_update_images_size;
	IQuery *q_set_done;
	IQuery *q_save_logdata;
	IQuery *q_get_unsent_logdata;
	IQuery *q_set_logdata_sent;
	IQuery *q_save_image_assoc;
	IQuery *q_get_users;
	IQuery *q_get_rights;
	IQuery *q_get_report_settings;
	IQuery *q_format_unixtime;
	IQuery *q_get_last_incremental_complete;


	int state;
	std::string t_name;

	int link_logcnt;

	IPipe *hashpipe;
	IPipe *hashpipe_prepare;

	ServerPingThread *pingthread;
	THREADPOOL_TICKET pingthread_ticket;

	SStatus status;
	bool has_error;

	bool r_incremental;
	
	bool can_backup_images;

	bool do_full_backup_now;
	bool do_incr_backup_now;
	bool do_update_settings;
	bool do_full_image_now;
	bool do_incr_image_now;

	static int running_backups;
	static int running_file_backups;
	static IMutex *running_backup_mutex;

	int filesrv_protocol_version;
	int file_protocol_version;
	int set_settings_version;
	volatile bool internet_connection;
	int image_protocol_version;
	int update_version;

	bool use_snapshots;
	bool use_reflink;
	bool use_tmpfiles;
	bool use_tmpfiles_images;

	CTCPStack tcpstack;

	IPipeThrottler *client_throttler;

	BackupServerHash *bsh;
	THREADPOOL_TICKET bsh_ticket;
	BackupServerPrepareHash *bsh_prepare;
	THREADPOOL_TICKET bsh_prepare_ticket;
	BackupServerHash *local_hash;

	int64 last_image_backup_try;
	size_t count_image_backup_try;

	int64 last_file_backup_try;
	size_t count_file_backup_try;

	std::string session_identity;
};
