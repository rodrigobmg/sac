/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/



// Licence: see TexturePacker.h
#include "TexturePacker.h"
#include <assert.h>
#include <iostream>

namespace TEXTURE_PACKER
{


class Rect
{
public:

  void set(int x1,int y1,int x2,int y2)
  {
    mX1 = x1;
    mY1 = y1;
    mX2 = x2;
    mY2 = y2;
  }

  bool intersects(const Rect &r) const
  {
    bool intersect = true;

    if ( mX2 < r.mX1 ||
         mX1 > r.mX2 ||
         mY2 < r.mY1 ||
         mY1 > r.mY2 )
    {
      intersect  = false;
    }
    return intersect;
  }
  int mX1;
  int mY1;
  int mX2;
  int mY2;
};

class Texture
{
public:
  void set(int wid,int hit)
  {
    mWidth = wid;
    mHeight = hit;
    mX = 0;
    mY = 0;
    mFlipped = false;
    mPlaced = false;
    mArea = wid*hit;
    if ( wid >= hit )
      mLongestEdge = wid;
    else
      mLongestEdge = hit;
  }

  bool isPlaced(void) const { return mPlaced; };

  void place(int x,int y,bool flipped)
  {
    mX = x;
    mY = y;
    mFlipped = flipped;
    mPlaced = true;
  }

  int mWidth;
  int mHeight;
  int mX;
  int mY;
  int mLongestEdge;
  int mArea;

  bool  mFlipped:1;       // true if the texture was rotated to make it fit better.
  bool  mPlaced:1;
};

class Node
{
public:
  Node(int x,int y,int wid,int hit)
  {
    mX = x;
    mY = y;
    mWidth = wid;
    mHeight = hit;
    mNext = 0;
  }

  Node *getNext(void) const { return mNext; };

  bool fits(int wid,int hit,int &edgeCount) const
  {
    bool ret = false;

    edgeCount = 0;

    if ( wid == mWidth  || hit == mHeight || wid == mHeight || hit == mWidth )
    {
      if ( wid == mWidth )
      {
        edgeCount++;
        if ( hit == mHeight )
          edgeCount++;
      }
      else if ( wid == mHeight )
      {
        edgeCount++;
        if ( hit == mWidth )
        {
          edgeCount++;
        }
      }
      else if ( hit == mWidth )
      {
        edgeCount++;
      }
      else if ( hit == mHeight )
      {
        edgeCount++;
      }
    }

    if ( wid <= mWidth && hit <= mHeight )
    {
      ret = true;
    }
    else if ( hit <= mWidth && wid <= mHeight )
    {
      ret = true;
    }

    return ret;
  }

  void getRect(Rect &r) const
  {
    r.set(mX,mY,mX+mWidth-1,mY+mHeight-1);
  }

  void validate(Node *n)
  {
    Rect r1;
    Rect r2;
    getRect(r1);
    n->getRect(r2);
    assert( !r1.intersects(r2) );
  }

  bool merge(const Node &n)
  {
    bool ret = false;

    Rect r1;
    Rect r2;

    getRect(r1);
    n.getRect(r2);
    r1.mX2++;
    r1.mY2++;
    r2.mX2++;
    r2.mY2++;

    // if we share the top edge then..
    if ( r1.mX1 == r2.mX1 && r1.mX2 == r2.mX2 && r1.mY1 == r2.mY2 )
    {
      mY = n.mY;
      mHeight+=n.mHeight;
      ret = true;
    }
    else if ( r1.mX1 == r2.mX1 && r1.mX2 == r2.mX2 && r1.mY2 == r2.mY1 ) // if we share the bottom edge
    {
      mHeight+=n.mHeight;
      ret = true;
    }
    else if ( r1.mY1 == r2.mY1 && r1.mY2 == r2.mY1 && r1.mX1 == r2.mX2 ) // if we share the left edge
    {
      mX = n.mX;
      mWidth+=n.mWidth;
      ret = true;
    }
    else if ( r1.mY1 == r2.mY1 && r1.mY2 == r2.mY1 && r1.mX2 == r2.mX1 ) // if we share the left edge
    {
      mWidth+=n.mWidth;
      ret = true;
    }

    return ret;
  }


  Node *mNext;
  int mX;
  int mY;
  int mWidth;
  int mHeight;
};

class MyTexturePacker : public TexturePacker
{
public:
  MyTexturePacker(void)
  {
    mTextureCount = 0;
    mTextures = 0;
    mLongestEdge = 0;
    mTotalArea = 0;
    mTextureIndex = 0;
    mFreeList = 0;
  }
  virtual ~MyTexturePacker(void)
  {
    reset();
  }

  void reset(void)
  {
    mTextureCount = 0;
    delete []mTextures;
    mTextures = 0;
    mLongestEdge = 0;
    mTotalArea = 0;
    mTextureIndex = 0;
    if ( mFreeList )
    {
      Node *next = mFreeList;
      while ( next )
      {
        Node *kill = next;
        next = next->getNext();
        delete kill;
      }
    }
  }

  virtual void  setTextureCount(int tcount) // number of textures to consider..
  {
    reset();
    mTextureCount = tcount;
    mTextures = new Texture[tcount];
  }

  virtual void  addTexture(int wid,int hit)  // add textures 0 - n
  {
    assert( mTextureIndex < mTextureCount );
    if ( mTextureIndex < mTextureCount )
    {
      mTextures[mTextureIndex].set(wid,hit);
      mTextureIndex++;
      if ( wid > mLongestEdge )  mLongestEdge = wid;
      if ( hit > mLongestEdge )  mLongestEdge = hit;
      mTotalArea+=(wid*hit);
    }
  }

  virtual void newFree(int x,int y,int wid,int hit)
  {
    Node *node = new Node(x,y,wid,hit);
    node->mNext = mFreeList;
    mFreeList = node;
  }

  int nextPow2(int v) const
  {
    int p = 1;
    while ( p < v )
    {
      p = p*2;
    }
    return p;
  }

  virtual int packTextures(int &width,int &height,bool forcePowerOfTwo,bool onePixelBorder, int forceWidth)  // pack the textures, the return code is the amount of wasted/unused area.
  {
    width = 0;
    height = 0;

    if ( onePixelBorder )
    {
      for (int i=0; i<mTextureCount; i++)
      {
        Texture &t = mTextures[i];
        t.mWidth+=2;
        t.mHeight+=2;
      }
      mLongestEdge+=2;
    }

    if ( forcePowerOfTwo )
    {
      mLongestEdge = nextPow2(mLongestEdge);
    }

youpi:
  if (forceWidth == 0) {
    width = mLongestEdge;
  }else {              // The width is no more than the longest edge of any rectangle passed in
    width = forceWidth;
  }
    int count = mTotalArea / (mLongestEdge);
    height = (count+2)*mLongestEdge;            // We guess that the height is no more than twice the longest edge.  On exit, this will get shrunk down to the actual tallest height.

    if (height > 1024 && width < 1024) {
        mLongestEdge *= 2;
        goto youpi;
    }

    mDebugCount = 0;
    newFree(0,0,width,height);

    // We must place each texture
    for (int i=0; i<mTextureCount; i++)
    {

      int index = 0;
      int longestEdge = 0;
      int mostArea = 0;

      // We first search for the texture with the longest edge, placing it first.
      // And the most area...
      for (int j=0; j<mTextureCount; j++)
      {

        Texture &t = mTextures[j];

        if ( !t.isPlaced() ) // if this texture has not already been placed.
        {
          if ( t.mLongestEdge > longestEdge )
          {
            mostArea = t.mArea;
            longestEdge = t.mLongestEdge;
            index = j;
          }
          else if ( t.mLongestEdge == longestEdge ) // if the same length, keep the one with the most area.
          {
            if ( t.mArea > mostArea )
            {
              mostArea = t.mArea;
              index = j;
            }
          }
        }
      }

      // For the texture with the longest edge we place it according to this criteria.
      //   (1) If it is a perfect match, we always accept it as it causes the least amount of fragmentation.
      //   (2) A match of one edge with the minimum area left over after the split.
      //   (3) No edges match, so look for the node which leaves the least amount of area left over after the split.
      Texture &t = mTextures[index];

      int leastY = 0x7FFFFFFF;
      int leastX = 0x7FFFFFFF;

      Node *previousBestFit = 0;
      Node *bestFit = 0;
      Node *previous = 0;
      Node *search = mFreeList;
      int edgeCount = 0;

      // Walk the singly linked list of free nodes
      // see if it will fit into any currently free space
      while ( search )
      {
        int ec;
        if ( search->fits(t.mWidth,t.mHeight,ec) ) // see if the texture will fit into this slot, and if so how many edges does it share.
        {
          if ( ec == 2 )
          {
            previousBestFit = previous;     // record the pointer previous to this one (used to patch the linked list)
            bestFit = search; // record the best fit.
            edgeCount = ec;
            break;
          }
          if ( search->mY < leastY )
          {
            leastY = search->mY;
            leastX = search->mX;
            previousBestFit = previous;
            bestFit = search;
            edgeCount = ec;
          }
          else if ( search->mY == leastY && search->mX < leastX )
          {
            leastX = search->mX;
            previousBestFit = previous;
            bestFit = search;
            edgeCount = ec;
          }
        }
        previous = search;
        search = search->mNext;
      }

      assert( bestFit ); // we should always find a fit location!

      if ( bestFit )
      {
        validate();

        switch ( edgeCount )
        {
          case 0:
            if ( t.mLongestEdge <= bestFit->mWidth )
            {
              bool flipped = false;

              int wid = t.mWidth;
              int hit = t.mHeight;

              if ( hit > wid )
              {
                wid = t.mHeight;
                hit = t.mWidth;
                flipped = true;
              }

              t.place(bestFit->mX,bestFit->mY,flipped); // place it.

              newFree(bestFit->mX,bestFit->mY+hit,bestFit->mWidth,bestFit->mHeight-hit);

              bestFit->mX+=wid;
              bestFit->mWidth-=wid;
              bestFit->mHeight = hit;
              validate();
            }
            else
            {

              assert( t.mLongestEdge <= bestFit->mHeight );

              bool flipped = false;

              int wid = t.mWidth;
              int hit = t.mHeight;

              if ( hit < wid )
              {
                wid = t.mHeight;
                hit = t.mWidth;
                flipped = true;
              }

              t.place(bestFit->mX,bestFit->mY,flipped); // place it.

              newFree(bestFit->mX,bestFit->mY+hit,bestFit->mWidth,bestFit->mHeight-hit);

              bestFit->mX+=wid;
              bestFit->mWidth-=wid;
              bestFit->mHeight = hit;
              validate();
            }
            break;
          case 1:
            {
              if ( t.mWidth == bestFit->mWidth )
              {
                t.place(bestFit->mX,bestFit->mY,false);
                bestFit->mY+=t.mHeight;
                bestFit->mHeight-=t.mHeight;
                validate();
              }
              else if ( t.mHeight == bestFit->mHeight )
              {
                t.place(bestFit->mX,bestFit->mY,false);
                bestFit->mX+=t.mWidth;
                bestFit->mWidth-=t.mWidth;
                validate();
              }
              else if ( t.mWidth == bestFit->mHeight )
              {
                t.place(bestFit->mX,bestFit->mY,true);
                bestFit->mX+=t.mHeight;
                bestFit->mWidth-=t.mHeight;
                validate();
              }
              else if ( t.mHeight == bestFit->mWidth )
              {
                t.place(bestFit->mX,bestFit->mY,true);
                bestFit->mY+=t.mWidth;
                bestFit->mHeight-=t.mWidth;
                validate();
              }
            }
            break;
          case 2:
            {
              bool flipped = t.mWidth != bestFit->mWidth || t.mHeight != bestFit->mHeight;
              t.place(bestFit->mX,bestFit->mY,flipped);
              if ( previousBestFit )
              {
                previousBestFit->mNext = bestFit->mNext;
              }
              else
              {
                mFreeList = bestFit->mNext;
              }
              delete bestFit;
              validate();
            }
            break;
        }
        while ( mergeNodes() ); // keep merging nodes as much as we can...
      }
    }

    height = 0;
    for (int i=0; i<mTextureCount; i++)
    {
      Texture &t = mTextures[i];
      if ( onePixelBorder )
      {
        t.mWidth-=2;
        t.mHeight-=2;
        t.mX++;
        t.mY++;
      }

      int y;
      if (t.mFlipped)
      {
        y = t.mY + t.mWidth;
      }
      else
      {
        y = t.mY + t.mHeight;
      }

      if ( y > height )
        height = y;
    }

    if ( forcePowerOfTwo )
    {
      height = nextPow2(height);
    }

    return (width*height)-mTotalArea;
  }

  bool mergeNodes(void)
  {
    Node *f = mFreeList;
    while ( f )
    {
      Node *prev = 0;
      Node *c = mFreeList;
      while ( c )
      {
        if ( f != c )
        {
          if ( f->merge(*c) )
          {
            assert(prev);
            prev->mNext = c->mNext;
            delete c;
            return true;
          }
        }
        prev = c;
        c = c->mNext;
      }
      f = f->mNext;
    }
    return false;
  }

  void validate(void)
  {
#if SAC_DEBUG
    Node *f = mFreeList;
    while ( f )
    {
      Node *c = mFreeList;
      while ( c )
      {
        if ( f != c )
        {
          f->validate(c);
        }
        c = c->mNext;
      }
      f = f->mNext;
    }
#endif
  }

  virtual bool  getTextureLocation(int index,int &x,int &y,int &wid,int &hit)
  {
    bool ret = false;

    x = y = wid = hit = 0;

    assert( index < mTextureCount );
    if ( index < mTextureCount )
    {
      Texture &t = mTextures[index];
      x = t.mX;
      y = t.mY;
      if ( t.mFlipped )
      {
        wid = t.mHeight;
        hit = t.mWidth;
      }
      else
      {
        wid = t.mWidth;
        hit = t.mHeight;
      }
      ret = t.mFlipped;
    }

    return ret;
  }


private:
  int    mDebugCount;
  Node    *mFreeList;
  int    mTextureIndex;
  int    mTextureCount;
  Texture *mTextures;
  int    mLongestEdge;
  int    mTotalArea;
};


TexturePacker * createTexturePacker(void)
{
  MyTexturePacker *m = new MyTexturePacker;
  return static_cast< TexturePacker *>(m);
}

void releaseTexturePacker(TexturePacker *tp)
{
  MyTexturePacker *m = static_cast< MyTexturePacker *>(tp);
  delete m;
}

}; // end of namepsace
