/* Copyright (C) 2006 by George Williams */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.

 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gdraw.h"
#include "ggadgetP.h"
#include <string.h>

#define GG_Glue		((GGadget *) -1)	/* Special entries */
#define GG_ColSpan	((GGadget *) -2)	/* for box elements */
#define GG_RowSpan	((GGadget *) -3)	/* Must match those in ggadget.h */

static GBox hvgroup_box = { /* Don't initialize here */ 0 };
static GBox hvbox_box = { /* Don't initialize here */ 0 };
static int ghvbox_inited = false;

static void _GHVBox_Init(void) {
    if ( ghvbox_inited )
return;
    _GGadgetCopyDefaultBox(&hvgroup_box);
    _GGadgetCopyDefaultBox(&hvbox_box);
    hvgroup_box.border_type = bt_engraved;
    hvbox_box.border_type = bt_none;
    hvbox_box.border_width = 0;
    hvgroup_box.border_shape = hvbox_box.border_shape = bs_rect;
    hvgroup_box.padding = 2;
    hvbox_box.padding = 0;
    hvgroup_box.flags = hvbox_box.flags = 0;
    hvgroup_box.main_background = COLOR_TRANSPARENT;
    hvgroup_box.disabled_background = COLOR_TRANSPARENT;
    _GGadgetInitDefaultBox("GHVBox.",&hvbox_box,NULL);
    _GGadgetInitDefaultBox("GGroup.",&hvgroup_box,NULL);
    ghvbox_inited = true;
}

static void GHVBox_destroy(GGadget *g) {
    GHVBox *gb = (GHVBox *) g;

#if 0		/* Bug here. closes window then children get destroyed first */
		/* So we shouldn't. But if program explicitly destroys the box */
		/* children hang around unless we kill them here. Sigh. */
		/*  I don't do the latter, so I don't support it */
    int i;

    for ( i=0; i<gb->rows*gb->cols; ++i )
	if ( gb->children[i]!=GG_Glue && gb->children[i]!=GG_ColSpan && gb->children[i]!=GG_RowSpan )
	    GGadgetDestroy(gb->children[i]);
#endif
    free(gb->children);
    _ggadget_destroy(g);
}

static void GHVBoxMove(GGadget *g, int x, int y) {
    GHVBox *gb = (GHVBox *) g;
    int offx = x-g->r.x, offy = y-g->r.y;
    int i;

    if ( gb->label!=NULL )
	GGadgetMove(gb->label,gb->label->inner.x+offx,gb->label->inner.y+offy);
    for ( i=0; i<gb->rows*gb->cols; ++i )
	if ( gb->children[i]!=GG_Glue && gb->children[i]!=GG_ColSpan && gb->children[i]!=GG_RowSpan )
	    GGadgetMove(gb->children[i],
		    gb->children[i]->r.x+offx, gb->children[i]->r.y+offy);
    _ggadget_move(g,x,y);
}

struct sizedata {
    int extra_space;		/* a default button has "extra_space" */
    int min;			/* This value includes padding */
    int sized;
    int allocated;
    int allglue;
};

struct sizeinfo {
    struct sizedata *cols;
    struct sizedata *rows;
    int label_height, label_width;
    int width, height;
    int minwidth, minheight;
};

static void GHVBoxGatherSizeInfo(GHVBox *gb,struct sizeinfo *si) {
    int i,c,r,spanc, spanr, totc,totr, max, plus, extra, es;
    GRect outer;

    memset(si,0,sizeof(*si));
    si->cols = gcalloc(gb->cols,sizeof(struct sizedata));
    si->rows = gcalloc(gb->rows,sizeof(struct sizedata));
    for ( c=0; c<gb->cols; ++c ) si->cols[c].allglue = true;
    for ( r=0; r<gb->rows; ++r ) si->rows[r].allglue = true;

    for ( r=0; r<gb->rows; ++r ) {
	for ( c=0; c<gb->cols; ++c ) {
	    GGadget *g = gb->children[r*gb->cols+c];
	    if ( g==GG_Glue ) {
		if ( c+1!=gb->cols && si->cols[c].min<gb->hpad ) si->cols[c].min = gb->hpad;
		if ( r+1!=gb->rows && si->rows[r].min<gb->vpad ) si->rows[r].min = gb->vpad;
	    } else if ( g==GG_ColSpan || g==GG_RowSpan )
		/* Skip it */;
	    else if ( (r+1<gb->rows && gb->children[(r+1)*gb->cols+c]==GG_RowSpan) ||
		      (c+1<gb->cols && gb->children[r*gb->cols+c+1]==GG_ColSpan))
		/* This gadget spans some columns or rows. Come back later */;
	    else {
		GGadgetGetDesiredSize(g,&outer,NULL);
		es = GBoxExtraSpace(g);
		if ( c+1!=gb->cols ) outer.width += gb->hpad;
		if ( r+1!=gb->rows ) outer.height += gb->vpad;
		si->cols[c].allglue = false;
		if ( si->cols[c].extra_space<es ) si->cols[c].extra_space=es;
		if ( si->cols[c].min<outer.width ) si->cols[c].min=outer.width;
		si->rows[r].allglue = false;
		if ( si->rows[r].min<outer.height ) si->rows[r].min=outer.height;
		if ( si->rows[r].extra_space<es ) si->rows[r].extra_space=es;
	    }
	}
    }

    for ( r=0; r<gb->rows; ++r ) {
	for ( c=0; c<gb->cols; ++c ) {
	    GGadget *g = gb->children[r*gb->cols+c];
	    if ( g==GG_Glue || g==GG_ColSpan || g==GG_RowSpan )
		/* Skip it */;
	    else if ( (r+1<gb->rows && gb->children[(r+1)*gb->cols+c]==GG_RowSpan) ||
		      (c+1<gb->cols && gb->children[r*gb->cols+c+1]==GG_ColSpan)) {
		si->rows[r].allglue = false;
		totr = si->rows[r].min;
		for ( spanr=1; r+spanr<gb->rows &&
			gb->children[(r+spanr)*gb->cols+c]==GG_RowSpan; ++spanr ) {
		    si->rows[r+spanr].allglue = false;
		    totr += si->rows[r+spanr].min;
		}
		si->cols[c].allglue = false;
		totc = si->cols[c].min;
		for ( spanc=1; c+spanc<gb->cols &&
			gb->children[r*gb->cols+c+spanc]==GG_ColSpan; ++spanc ) {
		    si->cols[c+spanc].allglue = false;
		    totc += si->cols[c+spanc].min;
		}
		GGadgetGetDesiredSize(g,&outer,NULL);
		es = GBoxExtraSpace(g);
		if ( c+spanc!=gb->cols ) outer.width += gb->hpad;
		if ( r+spanr!=gb->rows ) outer.height += gb->vpad;
		if ( outer.width>totc ) {
		    plus = (outer.width-totc)/spanc;
		    extra = (outer.width-totc-spanc*plus);
		    for ( i=0; i<spanc; ++i ) {
			si->cols[c+i].min += plus + (extra>0);
			--extra;
		    }
		}
		if ( outer.height>totr ) {
		    plus = (outer.height-totr)/spanr;
		    extra = (outer.height-totr-spanr*plus);
		    for ( i=0; i<spanr; ++i ) {
			si->rows[r+i].min += plus + (extra>0);
			--extra;
		    }
		}
		if ( es!=0 ) {
		    for ( i=0; i<spanc; ++i ) {
			if ( es>si->cols[c+i].extra_space )
			    si->cols[c+i].extra_space = es;
		    }
		    for ( i=0; i<spanr; ++i ) {
			if ( es>si->rows[r+i].extra_space )
			    si->rows[r+i].extra_space = es;
		    }
		}
	    }
	}
    }

    if ( gb->label!=NULL ) {
	GGadgetGetDesiredSize(gb->label,&outer,NULL);
	totc = 0;
	for ( c=0; c<gb->cols ; ++c )
	    totc += si->cols[c].min;
	outer.width += 20;		/* Set back on each side */
	if ( outer.width>totc ) {
	    plus = (outer.width-totc)/gb->cols;
	    extra = (outer.width-totc-gb->cols*plus);
	    for ( i=0; i<gb->cols; ++i ) {
		si->cols[i].min += plus + (extra>0);
		--extra;
	    }
	}
	si->label_height = outer.height;
	si->label_width = outer.width;
    }

    for ( max=c=0; c<gb->cols; ++c )
	si->cols[c].sized = si->cols[c].min;
    for ( max=r=0; r<gb->rows; ++r )
	si->rows[r].sized = si->rows[r].min;

    if ( gb->grow_col==gb_samesize ) {
	for ( max=c=0; c<gb->cols; ++c )
	    if ( max<si->cols[c].sized ) max = si->cols[c].sized;
	for ( c=0; c<gb->cols; ++c )
	    si->cols[c].sized = max;
    } else if ( gb->grow_col==gb_expandgluesame ) {
	for ( max=c=0; c<gb->cols; ++c )
	    if ( max<si->cols[c].sized && !si->cols[c].allglue ) max = si->cols[c].sized;
	for ( c=0; c<gb->cols; ++c )
	    if ( !si->cols[c].allglue ) si->cols[c].sized = max;
    }

    if ( gb->grow_row==gb_samesize ) {
	for ( max=r=0; r<gb->rows; ++r )
	    if ( max<si->rows[r].sized ) max = si->rows[r].sized;
	for ( r=0; r<gb->rows; ++r )
	    si->rows[r].sized = max;
    } else if ( gb->grow_row==gb_expandgluesame ) {
	for ( max=r=0; r<gb->rows; ++r )
	    if ( max<si->rows[r].sized && !si->rows[r].allglue ) max = si->rows[r].sized;
	for ( r=0; r<gb->rows; ++r )
	    if ( !si->rows[r].allglue ) si->rows[r].sized = max;
    }

    for ( i=si->width = si->minwidth = 0; i<gb->cols; ++i ) {
	si->width += si->cols[i].sized;
	si->minwidth += si->cols[i].min;
    }
    for ( i=0, si->height=si->minheight = si->label_height; i<gb->rows; ++i ) {
	si->height += si->rows[i].sized;
	si->minheight += si->rows[i].min;
    }
}

static void GHVBoxResize(GGadget *g, int width, int height) {
    struct sizeinfo si;
    GHVBox *gb = (GHVBox *) g;
    int bp = GBoxBorderWidth(g->base,g->box);
    int i,c,r,spanc, spanr, totc,totr, glue_cnt, plus, extra;
    int x,y;
    int old_enabled = GDrawEnableExposeRequests(g->base,false);

    GHVBoxGatherSizeInfo(gb,&si);
    width -= 2*bp; height -= 2*bp;
    gb->g.inner.x = gb->g.r.x + bp; gb->g.inner.y = gb->g.r.y + bp;
    if ( gb->label!=NULL ) {
	gb->label_height = si.label_height;
	g->inner.y += si.label_height;
    }

    if ( si.width!=width ) {
	if ( width<si.width ) {
	    for ( i=0; i<gb->cols; ++i )
		si.cols[i].sized = si.cols[i].min;
	    si.width = si.minwidth;
	    if ( width<si.width && gb->hpad>1 && gb->cols!=1 ) {
		int reduce_pad = (si.width-width+gb->cols-1)/(gb->cols-1);
		if ( reduce_pad>gb->hpad-1 ) reduce_pad = gb->hpad-1;
		for ( i=0; i<gb->cols-1; ++i )
		    si.cols[i].sized -= reduce_pad;
		si.width -= (gb->cols-1)*reduce_pad;
	    }
	}
	for ( i=glue_cnt=0; i<gb->cols; ++i )
	    if ( si.cols[i].allglue )
		++glue_cnt;
	if ( width!=si.width ) {
	    if ( glue_cnt!=0 && width>si.width &&
		    (gb->grow_col==gb_expandglue || gb->grow_col==gb_expandgluesame )) {
		plus = (width-si.width)/glue_cnt;
		extra = (width-si.width-glue_cnt*plus);
		for ( i=0; i<gb->cols; ++i ) if ( si.cols[i].allglue ) {
		    si.cols[i].sized += plus + (extra>0);
		    --extra;
		}
	    } else if ( gb->grow_col>=0 &&
		    si.cols[gb->grow_col].sized+(width-si.width)-gb->hpad>2 ) {
		si.cols[gb->grow_col].sized += width-si.width;
	    } else {
		plus = (width-si.width)/gb->cols;
		if ( width<si.width )
		    --plus;
		extra = (width-si.width-gb->cols*plus);
		for ( i=0; i<gb->cols; ++i ) {
		    si.cols[i].sized += plus + (extra>0);
		    --extra;
		}
	    }
	}
    }

    if ( si.height!=height ) {
	if ( height<si.height ) {
	    for ( i=0; i<gb->rows; ++i )
		si.rows[i].sized = si.rows[i].min;
	    si.height = si.minheight;
	    if ( height<si.height && gb->hpad>1 && gb->rows!=1 ) {
		int reduce_pad = (si.height-height+gb->rows-1)/(gb->rows-1);
		if ( reduce_pad>gb->hpad-1 ) reduce_pad = gb->hpad-1;
		for ( i=0; i<gb->rows-1; ++i )
		    si.rows[i].sized -= reduce_pad;
		si.height -= (gb->rows-1)*reduce_pad;
	    }
	}
	for ( i=glue_cnt=0; i<gb->rows; ++i )
	    if ( si.rows[i].allglue )
		++glue_cnt;
	if ( height!=si.height ) {
	    if ( glue_cnt!=0 && height>si.height &&
		    (gb->grow_row==gb_expandglue || gb->grow_row==gb_expandgluesame )) {
		plus = (height-si.height)/glue_cnt;
		extra = (height-si.height-glue_cnt*plus);
		for ( i=0; i<gb->rows; ++i ) if ( si.rows[i].allglue ) {
		    si.rows[i].sized += plus + (extra>0);
		    --extra;
		}
	    } else if ( gb->grow_row>=0 &&
		    si.rows[gb->grow_row].sized+(height-si.height)-gb->vpad>2 ) {
		si.rows[gb->grow_row].sized += height-si.height;
	    } else {
		plus = (height-si.height)/gb->rows;
		if ( height<si.height )
		    --plus;
		extra = (height-si.height-gb->rows*plus);
		for ( i=0; i<gb->rows; ++i ) {
		    si.rows[i].sized += plus + (extra>0);
		    --extra;
		}
	    }
	}
    }

    y = gb->g.inner.y;
    if ( gb->label!=NULL ) {
	GGadgetResize( gb->label, si.label_width, si.label_height);
	GGadgetMove( gb->label, gb->g.inner.x+10, y-si.label_height-bp/2);
    }
    for ( r=0; r<gb->rows; ++r ) {
	x = gb->g.inner.x;
	for ( c=0; c<gb->cols; ++c ) {
	    GGadget *g = gb->children[r*gb->cols+c];
	    if ( g==GG_Glue || g==GG_ColSpan || g==GG_RowSpan )
		/* Skip it */;
	    else {
		int xes, yes, es;
		totr = si.rows[r].sized;
		for ( spanr=1; r+spanr<gb->rows &&
			gb->children[(r+spanr)*gb->cols+c]==GG_RowSpan; ++spanr )
		    totr += si.rows[r+spanr].sized;
		totc = si.cols[c].sized;
		for ( spanc=1; c+spanc<gb->cols &&
			gb->children[r*gb->cols+c+spanc]==GG_ColSpan; ++spanc )
		    totc += si.cols[c+spanc].sized;
		if ( r+spanr!=gb->rows ) totr -= gb->vpad;
		if ( c+spanc!=gb->cols ) totc -= gb->hpad;
		es = GBoxExtraSpace(g);
		xes = si.cols[c].extra_space - es;
		yes = si.rows[r].extra_space - es;
		GGadgetResize(g,totc-2*xes,totr-2*yes);
		GGadgetMove(g,x+xes,y+yes);
	    }
	    x += si.cols[c].sized;
	}
	y += si.rows[r].sized;
    }

    free(si.cols); free(si.rows);

    gb->g.inner.width = width; gb->g.inner.height = height;
    gb->g.r.width = width + 2*bp; gb->g.r.height = height + 2*bp;
    GDrawEnableExposeRequests(g->base,old_enabled);
    GDrawRequestExpose(g->base,&g->r,false);
}

static void GHVBoxGetDesiredSize(GGadget *g, GRect *outer, GRect *inner) {
    struct sizeinfo si;
    GHVBox *gb = (GHVBox *) g;
    int bp = GBoxBorderWidth(g->base,g->box);

    GHVBoxGatherSizeInfo(gb,&si);

    if ( inner!=NULL ) {
	inner->x = inner->y = 0;
	inner->width = si.width; inner->height = si.height;
    }
    if ( outer!=NULL ) {
	outer->x = outer->y = 0;
	outer->width = si.width+2*bp; outer->height = si.height+2*bp;
    }
    free(si.cols); free(si.rows);
}

static int GHVBoxFillsWindow(GGadget *g) {
return( true );
}

static int expose_nothing(GWindow pixmap, GGadget *g, GEvent *event) {
    GHVBox *gb = (GHVBox *) g;
    GRect r;

    if ( gb->label==NULL )
	GBoxDrawBorder(pixmap,&g->r,g->box,g->state,false);
    else {
	r = g->r;
	r.y += gb->label_height/2;
	r.height -= gb->label_height/2;
	GBoxDrawBorder(pixmap,&r,g->box,g->state,false);
	/* Background is transperant */
	(gb->label->funcs->handle_expose)(pixmap,gb->label,event);
    }
return( true );
}

struct gfuncs ghvbox_funcs = {
    0,
    sizeof(struct gfuncs),

    expose_nothing,	/* Expose */
    _ggadget_noop,	/* Mouse */
    _ggadget_noop,	/* Key */
    NULL,
    NULL,		/* Focus */
    NULL,
    NULL,

    _ggadget_redraw,
    GHVBoxMove,
    GHVBoxResize,
    _ggadget_setvisible,
    _ggadget_setenabled,
    _ggadget_getsize,
    _ggadget_getinnersize,

    GHVBox_destroy,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    GHVBoxGetDesiredSize,
    NULL,
    GHVBoxFillsWindow,
    NULL
};

void GHVBoxSetExpandableCol(GGadget *g,int col) {
    GHVBox *gb = (GHVBox *) g;
    gb->grow_col = col;
}

void GHVBoxSetExpandableRow(GGadget *g,int row) {
    GHVBox *gb = (GHVBox *) g;
    gb->grow_row = row;
}

void GHVBoxSetPadding(GGadget *g,int hpad, int vpad) {
    GHVBox *gb = (GHVBox *) g;
    gb->hpad = hpad;
    gb->vpad = vpad;
}

static GHVBox *_GHVBoxCreate(struct gwindow *base, GGadgetData *gd,void *data,
	int hcnt, int vcnt, GBox *def_box) {
    GHVBox *gb = gcalloc(1,sizeof(GHVBox));
    int i, h, v;
    GGadgetCreateData *label = (GGadgetCreateData *) (gd->label);

    if ( !ghvbox_inited )
	_GHVBox_Init();

    gd->label = NULL;
    gb->g.funcs = &ghvbox_funcs;
    _GGadget_Create(&gb->g,base,gd,data,def_box);
    gb->rows = vcnt; gb->cols = hcnt;
    gb->grow_col = gb->grow_row = gb_expandall;
    gb->hpad = gb->vpad = 2;

    gb->g.takes_input = false; gb->g.takes_keyboard = false; gb->g.focusable = false;

    if ( label != NULL ) {
	gb->label = label->ret =
		(label->creator)(base,&label->gd,label->data);
    }

    gb->children = galloc(vcnt*hcnt*sizeof(GGadget *));
    for ( i=v=0; v<vcnt; ++v ) {
	for ( h=0; h<hcnt && gd->u.boxelements[i]!=NULL; ++h, ++i ) {
	    GGadgetCreateData *gcd = gd->u.boxelements[i];
	    if ( gcd==GCD_Glue ||
		    gcd==GCD_ColSpan ||
		    gcd==GCD_RowSpan )
		gb->children[v*hcnt+h] = (GGadget *) gcd;
	    else {
		gcd->gd.pos.x = gcd->gd.pos.y = 1;
		gb->children[v*hcnt+h] = gcd->ret =
			(gcd->creator)(base,&gcd->gd,gcd->data);
	    }
	}
	while ( h<hcnt )
	    gb->children[v*hcnt+h++] = GG_Glue;
	if ( gd->u.boxelements[i]==NULL )
	    ++i;
    }
#if 0
    GHVBoxGetDesiredSize(&gb->g,&outer,NULL);
    if ( gd->pos.width!=0 ) outer.width = gd->pos.width;
    if ( gd->pos.height!=0 ) outer.height = gd->pos.height;
    GHVBoxResize(&gb->g,outer.width,outer.height);
#endif
return( gb );
}

GGadget *GHBoxCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    GHVBox *gb;
    int hcnt;

    for ( hcnt=0; gd->u.boxelements[hcnt]!=NULL; ++hcnt );
    gb = _GHVBoxCreate(base,gd,data,hcnt,1,&hvbox_box);

return( &gb->g );
}

GGadget *GVBoxCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    GHVBox *gb;
    int vcnt;

    for ( vcnt=0; gd->u.boxelements[vcnt]!=NULL; ++vcnt );
    gb = _GHVBoxCreate(base,gd,data,1,vcnt,&hvbox_box);

return( &gb->g );
}

GGadget *GHVBoxCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    GHVBox *gb;
    int hcnt, vcnt, i;

    for ( hcnt=0; gd->u.boxelements[hcnt]!=NULL; ++hcnt );
    for ( i=0, vcnt=1; gd->u.boxelements[i]!=NULL || gd->u.boxelements[i+1]!=NULL ; ++i )
	if ( gd->u.boxelements[i]==NULL )
	    ++vcnt;
    gb = _GHVBoxCreate(base,gd,data,hcnt,vcnt,&hvbox_box);

return( &gb->g );
}

GGadget *GHVGroupCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    GHVBox *gb;
    int hcnt, vcnt, i;

    for ( hcnt=0; gd->u.boxelements[hcnt]!=NULL; ++hcnt );
    for ( i=0, vcnt=1; gd->u.boxelements[i]!=NULL || gd->u.boxelements[i+1]!=NULL ; ++i )
	if ( gd->u.boxelements[i]==NULL )
	    ++vcnt;
    gb = _GHVBoxCreate(base,gd,data,hcnt,vcnt,&hvgroup_box);

return( &gb->g );
}

void GHVBoxFitWindow(GGadget *g) {
    GRect outer;

    if ( !GGadgetFillsWindow(g)) {
	fprintf( stderr, "Call to GHVBoxFitWindow in something not an HVBox\n" );
return;
    }
    GHVBoxGetDesiredSize(g,&outer, NULL );
    /* Make any offset simmetrical */
    outer.width += 2*g->r.x;
    outer.height += 2*g->r.y;
    GDrawResize(g->base, outer.width, outer.height );
}