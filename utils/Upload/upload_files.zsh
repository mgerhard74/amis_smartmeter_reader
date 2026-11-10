#! /bin/zsh
## vim:set ft=zsh ts=4 sw=4 et:
set -e; unsetopt NULL_GLOB; LC_COLLATE=C


self=$0
selfdir=${self:h}


# --- set options
CURL=curl

CURL_OPTIONS=()
CURL_OPTIONS+=( --verbose )
#CURL_OPTIONS+=( -i )
#CURL_OPTIONS+=( --trace-ascii )

HOST=YOUR-AMIS-READER-IP
PROTO=http://
URL=/update
#AUTH_USER="admin"
#AUTH_PASS="admin"


# --- overrule options with local_config.zsh if available
if [[ -e ${selfdir}/local_config.zsh ]]; then
    source ${selfdir}/local_config.zsh
fi


# ------------ main() ------------
# Upload a single file with each curl

if [[ -z $@ ]]; then
    echo "${self:t} file1 file2 ..."
    echo "     Upload file(s) to ${PROTO}${HOST}${URL} using curl "
    exit 1
fi

local e=0
for f in $@; do
    if [[ ! -e ${f} ]]; then
        echo "File '${f}' not found"
        e=1
    fi
done
if [[ ${e} != 0 ]]; then
    exit 1
fi


for f in $@; do
    filename=${f}
    file_basename=${filename:t}


    # det -x # debugging on


    # start building a request and requestfile and send it to $HOST

    # possible auth things ....
    AUTH_HEADER=()
    if [[ -n "${AUTH_USER}" || -n "${AUTH_PASS}" ]]; then
        #-H 'authorization: Basic...'
        AUTH_HEADER=( --user "${AUTH_USER}:${AUTH_PASS}" )

        #-H 'authorization: Digest...'
        #AUTH_HEADER=( --digest --user "${AUTH_USER}:${AUTH_PASS}" )
    fi


    # -H 'content-type: application/x-www-form-urlencoded'
    # https://stackoverflow.com/questions/12667797/using-curl-to-upload-post-data-with-files
    #sha256sum ${filename}

    boundary=$(date | sha256sum | cut -f 1 -d ' ')

    #bindata=/tmp/tmp_binary${boundary}.data
    bindata=$(mktemp)


    MD5=$(md5sum ${filename} | cut -f 1 -d ' ')
    echo -n "--${boundary}\r\n" > ${bindata}
    echo -n 'Content-Disposition: form-data; name="MD5"\r\n' >> ${bindata}
    echo -n "\r\n" >> ${bindata}
    echo -n "${MD5}\r\n" >> ${bindata}

    SIZE=$(stat --format=%s ${filename})
    echo -n "--${boundary}\r\n" >> ${bindata}
    echo -n 'Content-Disposition: form-data; name="SIZE"\r\n' >> ${bindata}
    echo -n "\r\n" >> ${bindata}
    echo -n "${SIZE}\r\n" >> ${bindata}

    echo -n "--${boundary}\r\n" >> ${bindata}
    echo -n 'Content-Disposition: form-data; name="update"; filename="'${file_basename}'"'"\r\n"  >> ${bindata}
    #echo -n "Content-Transfer-Encoding: binary\r\n"  >> ${bindata}
    echo -n "Content-Type: application/octet-stream\r\n"  >> ${bindata}
    echo -n "\r\n" >> ${bindata}
    cat ${filename} >> ${bindata}
    echo -n "\r\n" >> ${bindata}


    # end of file(s)
    echo -n  "--${boundary}--\r\n" >> ${bindata}


    # vaidate request with:
    # sudo tcpdump -A dst HOST and dst port 80


    echo "Uploading: ${filename} to ${PROTO}${HOST}${URL}"
    echo "Details: Uploading file ${bindata}"

    ${CURL} ${CURL_OPTIONS} \
        "${AUTH_HEADER[@]}" \
        --header "Content-Type: multipart/form-data; boundary=${boundary}" \
        --data-binary "@${bindata}" \
        --url ${PROTO}${HOST}${URL} # | jq .
    echo ""

    # cleanup
    rm "${bindata}"
done
