--- cvsps.c.orig	2008-04-01 21:18:44.000000000 -0400
+++ cvsps.c	2008-04-01 21:18:44.000000000 -0400
@@ -2575,8 +2575,8 @@
     for (next = ps->members.next; next != &ps->members; next = next->next) 
     {
 	PatchSetMember * psm = list_entry(next, PatchSetMember, link);
-	rev = psm->pre_rev;
 	int d1, d2;
+	rev = psm->pre_rev;
 
 	/* the reason this is at all complicated has to do with a 
 	 * branch off of a branch.  it is possible (and indeed 
