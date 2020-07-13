/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*-------------------------------------------------------------------------
 *
 * Created:		H5ESint.c
 *			Apr  8 2020
 *			Quincey Koziol <koziol@lbl.gov>
 *
 * Purpose:		Internal "event set" routines for managing asynchronous
 *                      operations.
 *
 *                      Please see the asynchronous I/O RFC document
 *                      for a full description of how they work, etc.
 *
 *-------------------------------------------------------------------------
 */

/****************/
/* Module Setup */
/****************/

#include "H5ESmodule.h"         /* This source code file is part of the H5ES module */


/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5ESpkg.h"            /* Event Sets                           */
#include "H5FLprivate.h"        /* Free Lists                           */
#include "H5Iprivate.h"         /* IDs                                  */


/****************/
/* Local Macros */
/****************/


/******************/
/* Local Typedefs */
/******************/


/********************/
/* Package Typedefs */
/********************/

/* Typedef for event nodes */
struct H5ES_event_t {
    H5VL_object_t *request;             /* Request token for event */
    struct H5ES_event_t *prev, *next;   /* Previous and next event nodes */
};


/********************/
/* Local Prototypes */
/********************/
static herr_t H5ES__close_cb(void *es, void **request_token);


/*********************/
/* Package Variables */
/*********************/

/* Package initialization variable */
hbool_t H5_PKG_INIT_VAR = FALSE;


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/

/* Event Set ID class */
static const H5I_class_t H5I_EVENTSET_CLS[1] = {{
    H5I_EVENTSET,               /* ID class value */
    0,                          /* Class flags */
    0,                          /* # of reserved IDs for class */
    (H5I_free_t)H5ES__close_cb  /* Callback routine for closing objects of this class */
}};

/* Declare a static free list to manage H5ES_t structs */
H5FL_DEFINE_STATIC(H5ES_t);

/* Declare a static free list to manage H5ES_event_t structs */
H5FL_DEFINE_STATIC(H5ES_event_t);



/*-------------------------------------------------------------------------
 * Function:    H5ES__init_package
 *
 * Purpose:     Initializes any interface-specific data or routines.
 *
 * Return:      Non-negative on success / Negative on failure
 *
 * Programmer:	Quincey Koziol
 *	        Monday, April 6, 2020
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5ES__init_package(void)
{
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_PACKAGE

    /* Initialize the ID group for the event set IDs */
    if(H5I_register_type(H5I_EVENTSET_CLS) < 0)
        HGOTO_ERROR(H5E_EVENTSET, H5E_CANTINIT, FAIL, "unable to initialize interface")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES__init_package() */


/*-------------------------------------------------------------------------
 * Function:    H5ES_term_package
 *
 * Purpose:     Terminate this interface.
 *
 * Return:      Success:    Positive if anything is done that might
 *                          affect other interfaces; zero otherwise.
 *              Failure:    Negative
 *
 * Programmer:	Quincey Koziol
 *	        Monday, April 6, 2020
 *
 *-------------------------------------------------------------------------
 */
int
H5ES_term_package(void)
{
    int    n = 0;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    if(H5_PKG_INIT_VAR) {
        /* Destroy the event set ID group */
        n += (H5I_dec_type_ref(H5I_EVENTSET) > 0);

        /* Mark closed */
        if(0 == n)
            H5_PKG_INIT_VAR = FALSE;
    } /* end if */

    FUNC_LEAVE_NOAPI(n)
} /* end H5ES_term_package() */


/*-------------------------------------------------------------------------
 * Function:    H5ES__close_cb
 *
 * Purpose:     Called when the ref count reaches zero on an event set's ID
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *	        Monday, April 6, 2020
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5ES__close_cb(void *_es, void H5_ATTR_UNUSED **rt)
{
    H5ES_t *es = (H5ES_t *)_es;         /* The event set to close */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(es);

    /* Close the event set object */
    if(H5ES__close(es) < 0)
        HGOTO_ERROR(H5E_EVENTSET, H5E_CLOSEERROR, FAIL, "unable to close event set");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES__close_cb() */


/*-------------------------------------------------------------------------
 * Function:    H5ES__create
 *
 * Purpose:     Private function to create an event set object
 *
 * Return:      Success:    Pointer to an event set struct
 *              Failure:    NULL
 *
 * Programmer:	Quincey Koziol
 *              Wednesday, April 8, 2020
 *
 *-------------------------------------------------------------------------
 */
H5ES_t *
H5ES__create(void)
{
    H5ES_t *es = NULL;          /* Pointer to event set */
    H5ES_t *ret_value = NULL;   /* Return value */

    FUNC_ENTER_PACKAGE

    /* Allocate space for new event set */
    if(NULL == (es = H5FL_CALLOC(H5ES_t)))
        HGOTO_ERROR(H5E_EVENTSET, H5E_CANTALLOC, NULL, "can't allocate event set object")

    /* Set the return value */
    ret_value = es;

done:
    if(!ret_value)
        if(es && H5ES__close(es) < 0)
            HDONE_ERROR(H5E_EVENTSET, H5E_CANTRELEASE, NULL, "unable to free event set")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES__create() */


/*-------------------------------------------------------------------------
 * Function:    H5ES_insert
 *
 * Purpose:     Insert a request token into an event set
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *	        Wednesday, April 8, 2020
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5ES_insert(H5ES_t *es, H5VL_object_t *request)
{
    H5ES_event_t *ev;                   /* Event for request */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(es);
    HDassert(request);

    /* Allocate space for new event */
    if(NULL == (ev = H5FL_CALLOC(H5ES_event_t)))
        HGOTO_ERROR(H5E_EVENTSET, H5E_CANTALLOC, FAIL, "can't allocate event object")

    /* Set request for event */
    ev->request = request;

    /* Append event onto the event set's list */
    if(NULL == es->tail)
        es->head = es->tail = ev;
    else {
        ev->prev = es->tail;
        es->tail->next = ev;
        es->tail = ev;
    } /* end else */

    /* Increment the # of events in set */
    es->count++;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES_insert() */


/*-------------------------------------------------------------------------
 * Function:    H5ES__test
 *
 * Purpose:     Test if all operations in event set have completed
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *	        Monday, July 13, 2020
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5ES__test(const H5ES_t *es, H5ES_status_t *status)
{
    const H5ES_event_t *ev;             /* Event to check */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(es);
    HDassert(status);

    /* Iterate over the events in the set, testing them for completion */
    ev = es->head;
    if(ev) {
        /* If there are events in the event set, assume that they are in progress */
        *status = H5ES_STATUS_IN_PROGRESS;

        /* Iterate over events */
        while(ev) {
            H5ES_status_t ev_status = H5ES_STATUS_SUCCEED;      /* Status from event's operation */

            /* Test the request (i.e. wait for 0 time) */
            if(H5VL_request_wait(ev->request, 0, &ev_status) < 0)
                HGOTO_ERROR(H5E_EVENTSET, H5E_CANTWAIT, FAIL, "unable to test operation")

            /* Check for status values that indicate we should break out of the loop */
            if(ev_status == H5ES_STATUS_FAIL || ev_status == H5ES_STATUS_CANCELED) {
                *status = ev_status;
                break;
            } /* end if */

            /* Advance to next node */
            ev = ev->next;
        } /* end while */
    } /* end if */
    else
        /* If no operations in event set, assume they have successfully completed */
        *status = H5ES_STATUS_SUCCEED;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES__test() */


/*-------------------------------------------------------------------------
 * Function:    H5ES__wait
 *
 * Purpose:     Wait for operations in event set to complete
 *
 * Note:        Timeout value is in ns, and is per-operation, not for H5ES__wait
 *              itself.
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *	        Monday, July 13, 2020
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5ES__wait(H5ES_t *es, uint64_t timeout, H5ES_status_t *status)
{
    H5ES_event_t *ev;                   /* Event to operate on */
    hbool_t early_exit = FALSE;         /* Whether the loop exited early */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(es);

    /* Iterate over the events in the set, waiting for them to complete */
    ev = es->head;
    while(ev) {
        H5ES_event_t *tmp;              /* Temporary event */
        H5ES_status_t ev_status;        /* Status from event's operation */

        /* Get pointer to next node, so we can free this one */
        tmp = ev->next;

        /* Wait on the request */
        if(H5VL_request_wait(ev->request, timeout, &ev_status) < 0)
            HGOTO_ERROR(H5E_EVENTSET, H5E_CANTWAIT, FAIL, "unable to wait for operation")

        /* Check for non-success status values that indicate we should break out of the loop */
        if(ev_status != H5ES_STATUS_SUCCEED) {
            *status = ev_status;
            early_exit = TRUE;
            break;
        } /* end if */

        /* Free the request */
        if(H5VL_request_free(ev->request) < 0)
            HGOTO_ERROR(H5E_EVENTSET, H5E_CANTFREE, FAIL, "unable to free request")

        /* Free request VOL object */
        if(H5VL_free_object(ev->request) < 0)
            HGOTO_ERROR(H5E_EVENTSET, H5E_CANTFREE, FAIL, "unable to free VOL object")

        /* Free the event node */
        H5FL_FREE(H5ES_event_t, ev);

        /* Advance to next node */
        ev = tmp;
    } /* end while */

    /* Check for all operations completed */
    if(!early_exit)
        *status = H5ES_STATUS_SUCCEED;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES__wait() */


/*-------------------------------------------------------------------------
 * Function:    H5ES__close
 *
 * Purpose:     Destroy an event set object
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *	        Monday, April 6, 2020
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5ES__close(H5ES_t *es)
{
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(es);

    /* Wait on operations in the set to complete */
    if(H5ES__wait(es, H5ES_WAIT_FOREVER, NULL) < 0)
        HGOTO_ERROR(H5E_EVENTSET, H5E_CANTWAIT, FAIL, "can't wait on operations")

    /* Release the event set */
    es = H5FL_FREE(H5ES_t, es);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5ES__close() */
