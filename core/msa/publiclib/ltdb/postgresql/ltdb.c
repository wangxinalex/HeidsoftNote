#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "lttype.h"
#include "dbpostgresql.h"
#include "ltdb.h"


ltDbConn *ltDbConnect(char *pUser,char *pPassword,char *pService){
		PGconn *       tempHandle;
	char           conninfo[512];
	
	char caHostname[128];
  char caDbname[128];
  
  memset(conninfo,0,sizeof(conninfo));
  
  if(pService == NULL) {
       	 	strcpy(caHostname,"127.0.0.1");
        	strcpy(caDbname,pUser);
  }else {
        	char *p;
        	p = strstr(pService,"@");
        	if(*p == '@') {
           		 strcpy(caHostname,p+1);
           		 memset(caDbname,0,sizeof(caDbname));
            	 memcpy(caDbname,pService,p-pService);
        	}
  }
  sprintf(conninfo,"dbname='%s' user='%s' password='%s'",caDbname,pUser,pPassword);

  tempHandle = PQconnectdb(conninfo);
	if (PQstatus(tempHandle) != CONNECTION_OK)
  {
        PQfinish(tempHandle);
        return NULL;
  }
  
  return tempHandle;		
	
}

void  ltDbClose(ltDbConn *psConn){
	PQfinish(psConn);
}


ltDbCursor *ltDbOpenCursor(ltDbConn *pConn,  char *caSmt)
{
    ltDbCursor *tempCursor;
    int i;
    char       casql[2048];
    PGresult   *res;
    tempCursor=(ltDbCursor *)malloc(sizeof(ltDbCursor));
    if(tempCursor==NULL){
			return NULL;
    }
    sprintf(tempCursor->cursorname,"m%u%u",getpid(),time(0));
    tempCursor->db_field=NULL;
    tempCursor->num_field=0;
    res = PQexec(pConn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
                free(tempCursor);
                return NULL;
    }

    PQclear(res);
    sprintf(casql,"DECLARE %s CURSOR FOR %s",tempCursor->cursorname,caSmt);
    res = PQexec(pConn, casql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
                PQclear(res);
                res = PQexec(pConn, "END");
                PQclear(res);
                free(tempCursor);
                return NULL;
    }
    PQclear(res);

    sprintf(casql, "FETCH ALL in %s",tempCursor->cursorname);
    res = PQexec(pConn, casql );
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
                PQclear(res);
                res = PQexec(pConn, "END");
                PQclear(res);
                free(tempCursor);
                return NULL;
    }
    tempCursor->db_row=NULL;
    tempCursor->db_conn=pConn;
    tempCursor->nowline=0;
    tempCursor->maxline=PQntuples(res);
    tempCursor->db_cursor=res;
    tempCursor->num_field=PQnfields(res);
    tempCursor->db_field=(LT_DBFIELD)malloc(tempCursor->num_field*sizeof(char *));
    for(i = 0; i < tempCursor->num_field; i++)
	  {
	     	  unsigned int namelen;
		   		namelen=strlen(PQfname(res, i));
         	tempCursor->db_field[i] =(char *)malloc(namelen+1);
		   		memcpy(tempCursor->db_field[i],PQfname(res, i),namelen);
		   		tempCursor->db_field[i][namelen]=0;
	  }
	  return tempCursor;
       
}


void ltDbCloseCursor(ltDbCursor *ltCursor)
{
    int iTemp;
    if(ltCursor) {
			/*释放字段名*/
			if(ltCursor->db_field){
				if(ltCursor->num_field>0){
					for(iTemp=0;iTemp<ltCursor->num_field;iTemp++){
						if(ltCursor->db_field[iTemp]){
							free(ltCursor->db_field[iTemp]);
						}
					}
				}
				free(ltCursor->db_field);
				ltCursor->db_field=NULL;
			}
			if(ltCursor->db_row){
						free(ltCursor->db_row);
						ltCursor->db_row=NULL;
			}
			/*释放数据库光标*/
			if(ltCursor->db_cursor){
				  char       casql[128];
				  PGresult   *res;
		      PQclear(ltCursor->db_cursor);
	        /* 关闭入口，我们不用关心错误检查... */
	        sprintf(casql,"CLOSE %s",ltCursor->cursorname);
	        res = PQexec(ltCursor->db_conn, casql);
          PQclear(res);

	        /* 提交事务 */
          res = PQexec(ltCursor->db_conn, "END");
          PQclear(res);
			}
			/*释放指针*/
			free(ltCursor);
    }
    return ;
}



/*光标的fetch处理*/
unsigned  int ltNumField(ltDbCursor *ltCursor){
	if(ltCursor){
		return ltCursor->num_field;
	}
	return 0;
}

/*得到字段名*/
LT_DBFIELD  ltDbFetchField(ltDbCursor *ltCursor){
	if(ltCursor==NULL){
		return NULL;
	}
	return ltCursor->db_field;
}

/*fetch并得到字段值*/
LT_DBROW  ltDbFetchRow(ltDbCursor *ltCursor){
	LT_DBROW tmp;
	unsigned int fieldnum;
	int  iReturn;
	if(ltCursor==NULL){
		return NULL;
	}
	if(ltCursor->nowline>=ltCursor->maxline){
	  return NULL;
	}
	/*释放字段值*/
  if(ltCursor->db_row){
		free(ltCursor->db_row);
		ltCursor->db_row=NULL;
  }
  fieldnum=ltCursor->num_field;
  tmp=(LT_DBROW)malloc(fieldnum*sizeof(char *));
  for(iReturn=0;iReturn<fieldnum;iReturn++){
        tmp[iReturn]=PQgetvalue(ltCursor->db_cursor, ltCursor->nowline, iReturn); 
  }
  ltCursor->nowline++;
  ltCursor->db_row=tmp;	
	return ltCursor->db_row;
}


//void ltDbFreeRow(LT_DBROW dbRow){
//	int iTemp;
//	if(dbRow){
//		free(dbRow);
//		dbRow=NULL;
//  }
//}


/*更新操作*/
/*int ltDbExecSql(ltDbConn *pConn,  char *pSmt,...){
	va_list ap;
	char caSmt[4096];
	va_start(ap,pSmt);
	vsprintf(caSmt,pSmt,ap);
	va_end(ap);
	return mysql_query(pConn, caSmt) ;
}*/
int ltDbExecSql(ltDbConn *pConn,  char *pSmt){
  PGresult   *res;
	res = PQexec(pConn, pSmt) ;
  if(PQresultStatus(res) != PGRES_COMMAND_OK) //成功完成一个不返回数据的命令
  {
            PQclear(res);
            return -1;
  }
  PQclear(res);
  return 0;
}


