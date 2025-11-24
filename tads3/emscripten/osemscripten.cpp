#include <ctype.h>

#include <osifctyp.h>
#include <osunixt.h>
#include <osifc.h>

extern "C"{
/*
 *   prompt for information through a dialog
 */
int os_input_dialog(int /*icon_id*/,
                    const char *prompt, int standard_button_set,
                    const char **buttons, int button_count,
                    int /*default_index*/, int /*cancel_index*/)
{
    /* keep going until we get a valid response */
    for (;;)
    {
        int i;
        char buf[256];
        const char *p;
        const char *cur;
        char *resp;
        int match_cnt;
        int last_found;
        static const struct
        {
            const char *buttons[3];
            int button_count;
        } std_btns[] =
        {
            { { "&OK" },                    1 },
            { { "&OK", "&Cancel" },         2 },
            { { "&Yes", "&No" },            2 },
            { { "&Yes", "&No", "&Cancel" }, 3 }
        };

        /* 
         *   if we have a standard button set selected, get our button
         *   labels 
         */
        switch(standard_button_set)
        {
        case 0:
            /* use the explicit buttons provided */
            break;

        case OS_INDLG_OK:
            i = 0;

        use_std_btns:
            /* use the selected standard button set */
            buttons = (const char **)std_btns[i].buttons;
            button_count = std_btns[i].button_count;
            break;

        case OS_INDLG_OKCANCEL:
            i = 1;
            goto use_std_btns;

        case OS_INDLG_YESNO:
            i = 2;
            goto use_std_btns;

        case OS_INDLG_YESNOCANCEL:
            i = 3;
            goto use_std_btns;

        default:
            /* 
             *   we don't recognize other standard button sets - return an
             *   error 
             */
            return 0;
        }

        /* 
         *   if there are no buttons defined, they'll never be able to
         *   respond, so we'd just loop forever - rather than let that
         *   happen, return failure 
         */
        if (button_count == 0)
            return 0;

        /* display a newline and the prompt string */
        os_printz("\n");
        os_printz(prompt);

        /* display the response */
        for (i = 0 ; i < button_count ; ++i)
        {
            /* 
             *   display a slash to separate responses, if this isn't the
             *   first one 
             */
            if (i != 0)
                os_printz("/");

            /* get the current button */
            cur = buttons[i];

            /* 
             *   Look for a "&" in the response string.  If we find it,
             *   remove the "&" and enclose the shortcut key in parens.  
             */
            for (p = cur ; *p != '&' && *p != '\0' ; ++p) ;

            /* if we found the "&", put the next character in parens */
            if (*p == '&')
            {
                size_t pre_len;
                size_t post_len;

                /* limit the prefix length to avoid overflowing the buffer */
                pre_len = p - cur;
                if (pre_len > sizeof(buf) - 5)
                    pre_len = sizeof(buf) - 5;

                /* limit the postfix length to avoid buffer overflow, too */
                post_len = strlen(p + 2);
                if (post_len > sizeof(buf) - 5 - pre_len)
                    post_len = sizeof(buf) - 5 - pre_len;

                /* reformat the response string */
                sprintf(buf, "%.*s(%c)%.*s",
                        (int)pre_len, cur, *(p + 1), (int)post_len, p + 2);

                /* show it */
                os_printz(buf);
            }
            else
            {
                /* no '&' - just display the response string as-is */
                os_printz(cur);
            }
        }

        /* read the response */
        os_printz(" > ");
        os_gets((unsigned char *)buf, sizeof(buf));

        /* skip any leading spaces in the reply */
        for (resp = buf ; isspace(*resp) ; ++resp) ;

        /* if it's one character, check it against the shortcut keys */
        if (strlen(resp) == 1)
        {
            /* scan the responses */
            for (i = 0 ; i < button_count ; ++i)
            {
                /* look for a '&' in this button */
                for (p = buttons[i] ; *p != '&' && *p != '\0' ; ++p) ;

                /* if we found the '&', check the shortcut */
                if (*p == '&' && toupper(*(p+1)) == toupper(*resp))
                {
                    /* 
                     *   this is the one - return the current index
                     *   (bumping it by one to get a 1-based value) 
                     */
                    return i + 1;
                }
            }
        }

        /* 
         *   Either it's not a one-character reply, or it didn't match a
         *   short-cut - check it against the leading substrings of the
         *   responses.  If it matches exactly one of the responses in its
         *   leading substring, use that response.  
         */
        for (i = 0, match_cnt = 0 ; i < button_count ; ++i)
        {
            const char *p1;
            const char *p2;

            /* 
             *   compare this response to the user's response; skip any
             *   '&' in the button label 
             */
            for (p1 = resp, p2 = buttons[i] ; *p1 != '\0' && *p2 != '\0' ;
                 ++p1, ++p2)
            {
                /* if this is a '&' in the button label, skip it */
                if (*p2 == '&')
                    ++p2;

                /* if these characters don't match, it's no match */
                if (toupper(*p1) != toupper(*p2))
                    break;
            }

            /* 
             *   if we reached the end of the user's response, we have a
             *   match in the leading substring - count it and remember
             *   this as the last one, but keep looking, since we need to
             *   make sure we don't have any other matches 
             */
            if (*p1 == '\0')
            {
                ++match_cnt;
                last_found = i;
            }
        }

        /* 
         *   if we found exactly one match, return it (adjusting to a
         *   1-based index); if we found more or less than one match, it's
         *   not a valid response, so start over with a new prompt 
         */
        if (match_cnt == 1)
            return last_found + 1;
    }
}

static const char *S_res_strings[] =
{
    /* 
     *   OK/Cancel/Yes/No button labels.  We provide a shortcut for every
     *   label, to make the buttons work well in character mode. 
     */
    "&OK",                                              /* RESID_BTN_OK = 1 */
    "&Cancel",                                      /* RESID_BTN_CANCEL = 2 */
    "&Yes",                                            /* RESID_BTN_YES = 3 */
    "&No",                                              /* RESID_BTN_NO = 4 */

    /* reply strings for yorn() */
    "[Yy].*",                                             /* RESID_YORN_YES */
    "[Nn].*"                                               /* RESID_YORN_NO */
};


/*
 *   Load a string resource
 */
int os_get_str_rsc(int id, char *buf, size_t buflen)
{
    /* 
     *   make sure the index is in range of our array - note that the
     *   index values start at 1, but our array starts at 0, so adjust
     *   accordingly 
     */
    if (id < 0 || id >= (int)sizeof(S_res_strings)/sizeof(S_res_strings[0]))
        return 1;

    /* make sure the buffer is large enough */
    if (buflen < strlen(S_res_strings[id-1]) + 1)
        return 2;

    /* copy the string */
    strcpy(buf, S_res_strings[id-1]);

    /* return success */
    return 0;
}

}

void os_init_ui_after_load(class CVmBifTable *, class CVmMetaTable *)
{
}
