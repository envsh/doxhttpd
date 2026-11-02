


curdir=$PWD
for p in `echo "bookmark_mgr hash_calc dotfile_cleaner fmt_codec mirror_mgr sqlitebrowser"`; do
	echo "$p"
	cd "$p" && sh ./buildqt3.sh || cd "$curdir" 
done
