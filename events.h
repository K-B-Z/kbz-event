#ifndef __events_h__
#define __events_h__

#include <semaphore.h>

enum APP_EVENT_TYPE {
	EVENT_INPUT     = 1,
	EVENT_NOTIFY    = 2,
	EVENT_UPNP      = 3,
};
enum APP_VAL_TYPE{
	APP_VAL_INT		= 0,
	APP_VAL_STR		= 1,
	APP_VAL_PNT		= 2,
};

#define MAX_EVENTS_ENTRY		32
#define MAX_ACKS_ENTRY			8
#define MAX_PTHREAD		10

#define APP_STR_SIZE  512

enum APP_EVENT_ACTION {
	ACTION_NULL             = 0,
	/* INPUT_EVENT */
	ACTION_MODE             = 1,
	ACTION_PLAY,
	ACTION_PAUSE,
	ACTION_PLAYPAUSE,
	ACTION_STOP,
	ACTION_FORWARD,
	ACTION_BACK,
	ACTION_VOLUMEUP,
	ACTION_VOLUMEDOWN,
	ACTION_VOLUME,
	/* EVENT_NOTIFY */
	ACTION_LINE_CONNECTED,
	ACTION_LINE_DISCONNECTED,
	ACTION_CHARGE_IN,
	ACTION_CHARGE_DONE,
	ACTION_LOW_POWER,
	ACTION_WIFI_NEED,
	ACTION_WIFI_CONNECTED,
	ACTION_WIFI_DISCONNECTED,
	ACTION_NETWORK_ONLINE,
	ACTION_NETWORK_OFFLINE,
	ACTION_WAN_NETWORK_ONLINE,
	ACTION_WAN_NETWORK_OFFLINE,
	ACTION_SYSTEM_SHUTDOWN,
	ACTION_SYSTEM_REBOOT,
	ACTION_SYSTEM_REBOOT_RECOVERY,
	/* EVENT_UPNP */

	ACTION_AUDIO_START_I = 10000,
	ACTION_AUDIO_PLAY = 17262,
	ACTION_AUDIO_PLAYDONE = 13213,
	ACTION_AUDIO_STOP = 16235,
	ACTION_AUDIO_STOP_NO_PLAYDONE = 16123,
	ACTION_AUDIO_PAUSE = 18283,
	ACTION_AUDIO_RESUME = 11653,
	ACTION_AUDIO_PAUSE_RESUME_TOGGLE = 14412,
	ACTION_AUDIO_PAUSE_STAT = 11238,
	ACTION_AUDIO_VOLUME = 12382,
	ACTION_AUDIO_VOLUME_UP = 17634,
	ACTION_AUDIO_VOLUME_DOWN = 11282,
	ACTION_AUDIO_EQ = 17464,
	ACTION_AUDIO_PLAY_POS = 18162,
	ACTION_AUDIO_PLAY_DUR = 19172,
	ACTION_AUDIO_END_I = 19999,

	ACTION_RADIO_CTRL = 21111,
	ACTION_RADIO_PREV = 31282,
	ACTION_RADIO_NEXT = 31283,
	ACTION_RADIO_PLAY_DIR = 31237,
};

typedef struct {
	int action;
	char data[0];
} app_event_t;

typedef struct app_ack
{
	int sem_id;
	int action;
	int retval;
	enum APP_VAL_TYPE val_t;
	union {
		char	s[APP_STR_SIZE]; 
		int		i; 
		void*	d;
	} val;
} app_ack_t;

typedef struct shm_sem_type{
	int  shm_sem_type;
	char sem_name[20];
	char shm_name[32];
} ss_type_t;

enum SHM_SEM_TYPE {
	SHM_SEM_COMMON 	= 0,
	SHM_SEM_SYSTEM 	= 1,
	SHM_SEM_AUDIO   = 2,
	SHM_SEM_UPNP 	= 3,
	SHM_SEM_RADIO   = 4,
	SHM_SEM_PLAYER  = 5,
};
typedef struct event_share {
	char shm_name[32];
	int init_tag;
	int got;
	int index_get;
	int index_post;
	int count;
	int	  acks_use[MAX_ACKS_ENTRY];
	app_ack_t acks[MAX_ACKS_ENTRY];
	app_event_t entry[MAX_EVENTS_ENTRY];
}event_share_t;

typedef struct event_manager {
	int mode;
	ss_type_t type;
	sem_t *sem_event;
	sem_t *ack_sems[MAX_ACKS_ENTRY];
	event_share_t *es;
} event_manager_t;


#define SEM_EVENT "SEM_EVENT"

///////////////////////////////////////

int init_event(event_manager_t **pem,int mode);
int uninit_event(event_manager_t *em);
int post_event(event_manager_t *em, app_event_t event);
int get_event(event_manager_t *em, app_event_t* event);
int get_info(event_manager_t *em);

int push_event(event_manager_t *em, app_event_t event, int *retval);
int ack_event(event_manager_t *em, app_event_t* event, int retval);

#endif /* __events_h__ */
