//
//  main.c
//  Jason
//
//  Created by Timothy Smale on 10/10/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#include "jason.h"
#include <stdio.h>

int main(int argc, const char * argv[]) {
    const char *json = "{\"Users\":[{\"Name\":\"John\",\"Age\":25,\"Salary\":20000},{\"Name\":\"Mary\",\"Age\":42,\"Salary\":45000}]}";
    
    jason jason;
    memset(&jason, 0, sizeof(jason));
    
    jasonStatus status = jason_Deserialize(&jason, json, (int32_t)strlen(json));
    
    if(status != jasonStatus_Finished)
    {
        printf("%s: \"%.50s...\"\n", jasonStatus_Describe(status), jason.ParsePosition);
    }
    else
    {
        jasonValue *usersList = jason_HashLookup(&jason, jason.RootValue, "Users", strlen("Users"));
        if(usersList != NULL)
        {
            for(jasonValue *user = jasonValue_GetFirstChild(usersList); user != NULL; user = jasonValue_GetNextSibling(user))
            {
                jasonValue *name = jason_HashLookup(&jason, user, "Name", strlen("Name"));
                jasonValue *age = jason_HashLookup(&jason, user, "Age", strlen("Age"));
                jasonValue *salary = jason_HashLookup(&jason, user, "Salary", strlen("Salary"));
                
                if(name != NULL)
                {
                    printf("Name: %.*s\n", jasonValue_GetValueLen(name), jasonValue_GetValue(name));
                }
                
                if(age != NULL)
                {
                    printf("Age: %i\n", atoi(jasonValue_GetValue(age)));
                }
                
                if(salary != NULL)
                {
                    printf("Salary: %f\n", atof(jasonValue_GetValue(salary)));
                }
            }
        }
    }
    
    jason_Cleanup(&jason);
    return 0;
}
