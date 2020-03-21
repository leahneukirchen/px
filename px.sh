px() {
  ps wwp ${$(pgrep -d, "${(j:|:)@}"):?no matches} \
         -o pid,user:6,%cpu,%mem,vsz:10,rss:10,bsdstart,etime:12,bsdtime,args |
    sed '1s/     \(VSZ\|RSS\)/\1/g' |
    numfmt --header --field 5,6 --from-unit=1024 --to=iec --format "%5f"
}

