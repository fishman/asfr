/*

	status.h

	Header definitions for the Status modules ( status.cxx ).

*/


#ifndef STATUS_HPP
#define STAUTS_HPP

int status(struct JOB_PARM *My_Job);
int savestatus(struct JOB_PARM *My_Job);
int initstatus(struct JOB_PARM *My_Job);

#endif // STATUS_HPP

