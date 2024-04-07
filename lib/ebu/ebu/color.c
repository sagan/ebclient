/*
 * Copyright (c) 2020  Kazuhiro Ito
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "build-pre.h"
#include "eb.h"
#include "error.h"
#include "build-post.h"

/*
 * Examine whether the current subbook in `book' has color chart or
 * not.
 */
int
eb_have_color_chart(EB_Book *book)
{
    int result = 0;
    
    eb_lock(&book->lock);
    LOG(("in: eb_have_color_chart(book=%d)", (int)book->code));

    /*
     * Current subbook must have been set.
     */
    if (book->subbook_current == NULL)
	goto failed;

    /*
     * Check for the index page of color chart.
     */
    if (book->subbook_current->color_chart.start_page == 0)
	goto failed;

    result = 1;

  failed:
    LOG(("out: eb_have_color_chart() = %d", result));
    eb_unlock(&book->lock);
    return result;
}


/* return beginning POSITION of color chart. */
EB_Error_Code
eb_color_chart(EB_Book *book, EB_Position *position)
{
    EB_Error_Code error_code;
    int page;

    eb_lock(&book->lock);
    LOG(("in: eb_color_chart(book=%d)", (int)book->code));

    /*
     * Current subbook must have been set.
     */
    if (book->subbook_current == NULL) {
	error_code = EB_ERR_NO_CUR_SUB;
	goto failed;
    }

    /*
     * Check for the page number of color chart.
     */
    page = book->subbook_current->color_chart.start_page;
    if (page == 0) {
	error_code = EB_ERR_NO_SUCH_SEARCH;
	goto failed;
    }

    position->page = page;
    position->offset = 0;

    LOG(("out: eb_color_chart(position={%d,%d}) = %s",
	 position->page, position->offset, eb_error_string(EB_SUCCESS)));
    eb_unlock(&book->lock);

    return EB_SUCCESS;

  failed:
    LOG(("out: eb_color_chart() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}



/*
 * Write NUMBERth color's Munsell value (in ascii) into buffer VALUE.
 * buffer needs EB_MAX_COLOR_VALUE_LENGTH + 1 bytes.
 * NUMBER starts from 1.
 */
EB_Error_Code
eb_color_value(EB_Book *book, int number, char *value)
{
    EB_Error_Code error_code;
    int page;

    eb_lock(&book->lock);
    LOG(("in: eb_color_value(book=%d, number=%d)", (int)book->code, number));

    /*
     * Current subbook must have been set.
     */
    if (book->subbook_current == NULL) {
	error_code = EB_ERR_NO_CUR_SUB;
	goto failed;
    }

    /*
     * Check for the page number of color chart.
     */
    page = book->subbook_current->color_chart.start_page;
    if (page == 0) {
	error_code = EB_ERR_NO_SUCH_SEARCH;
	goto failed;
    }

    page += (number -1);
    if (number < 1 || page > book->subbook_current->color_chart.end_page) {
	error_code = EB_ERR_NO_SUCH_COLOR;
	goto failed;
    }

    if (zio_lseek(&book->subbook_current->text_zio,
		  (page - 1) * EB_SIZE_PAGE, SEEK_SET) == -1) {
	error_code = EB_ERR_FAIL_SEEK_TEXT;
	goto failed;
    }

    if (zio_read(&book->subbook_current->text_zio, value,
		 EB_MAX_COLOR_VALUE_LENGTH) < EB_MAX_COLOR_VALUE_LENGTH) {
	error_code = EB_ERR_FAIL_READ_TEXT;
	goto failed;
    }

    value[EB_MAX_COLOR_VALUE_LENGTH] = 0;
    eb_ebcdic037_to_ascii(value, value);

    error_code = EB_SUCCESS;

  failed:
    LOG(("out: eb_color_value() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}

/* 
 * Write NUMBERth color name (in EUC-JP) into buffer NAME.
 * Buffer needs (EB_MAX_COLOR_NAME_LENGTH + 1) bytes.
 * NUMBER starts from 1.
 */
EB_Error_Code
eb_color_name(EB_Book *book, int number, char *name)
{
    EB_Error_Code error_code;
    int page;
    ssize_t length;

    eb_lock(&book->lock);
    LOG(("in: eb_color_name(book=%d, number=%d)", (int)book->code, number));

    /*
     * Current subbook must have been set.
     */
    if (book->subbook_current == NULL) {
	error_code = EB_ERR_NO_CUR_SUB;
	goto failed;
    }

    /*
     * Check for the page number of color chart.
     */
    page = book->subbook_current->color_chart.start_page;
    if (page == 0) {
	error_code = EB_ERR_NO_SUCH_SEARCH;
	goto failed;
    }

    page += (number -1);
    if (number < 1 || page > book->subbook_current->color_chart.end_page) {
	error_code = EB_ERR_NO_SUCH_COLOR;
	goto failed;
    }

    if (zio_lseek(&book->subbook_current->text_zio,
		  (page - 1) * EB_SIZE_PAGE + EB_MAX_COLOR_VALUE_LENGTH,
		  SEEK_SET) == -1) {
	error_code = EB_ERR_FAIL_SEEK_TEXT;
	goto failed;
    }
    
    if (zio_read(&book->subbook_current->text_zio, name,
		 EB_MAX_COLOR_NAME_LENGTH) < EB_MAX_COLOR_NAME_LENGTH) {
	error_code = EB_ERR_FAIL_READ_TEXT;
	goto failed;
    }

    name[EB_MAX_COLOR_NAME_LENGTH] = 0;
    eb_jisx0208_to_euc(name, name);

    error_code = EB_SUCCESS;

  failed:
    LOG(("out: eb_color_name() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}

/*
 * Write NUMBERth color page dump into buffer BUFFER.
 * Buffer needs EB_SIZE_PAGE bytes.
 * NUMBER starts from 1.
 */
EB_Error_Code
eb_color_page(EB_Book *book, int number, char *buffer)
{
    EB_Error_Code error_code;
    int page;

    eb_lock(&book->lock);
    LOG(("in: eb_color_page(book=%d, number=%d)", (int)book->code, number));

    /*
     * Current subbook must have been set.
     */
    if (book->subbook_current == NULL) {
	error_code = EB_ERR_NO_CUR_SUB;
	goto failed;
    }

    /*
     * Check for the page number of color chart.
     */
    page = book->subbook_current->color_chart.start_page;
    if (page == 0) {
	error_code = EB_ERR_NO_SUCH_SEARCH;
	goto failed;
    }

    page += (number - 1);
    if (number < 1 || page > book->subbook_current->color_chart.end_page) {
	error_code = EB_ERR_NO_SUCH_COLOR;
	goto failed;
    }

    if (zio_lseek(&book->subbook_current->text_zio,
		  (page - 1) * EB_SIZE_PAGE + EB_MAX_COLOR_VALUE_LENGTH,
		  SEEK_SET) == -1) {
	error_code = EB_ERR_FAIL_SEEK_TEXT;
	goto failed;
    }
    
    if (zio_read(&book->subbook_current->text_zio, buffer, EB_SIZE_PAGE) <
	EB_SIZE_PAGE) {
	error_code = EB_ERR_FAIL_READ_TEXT;
	goto failed;
    }

    error_code = EB_SUCCESS;

    /*
     * An error occurs...
     */
  failed:
    LOG(("out: eb_color_name() = %s", eb_error_string(error_code)));
    eb_unlock(&book->lock);
    return error_code;
}
