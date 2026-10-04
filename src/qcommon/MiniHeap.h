#if !defined(MINIHEAP_H_INC)
#define MINIHEAP_H_INC


class CMiniHeap
{
private:
	char	*mHeap;
	char	*mCurrentHeap;
	size_t	mSize;
public:

// the heap owns its buffer, so it can't be copied
CMiniHeap(const CMiniHeap &) = delete;
CMiniHeap &operator=(const CMiniHeap &) = delete;

// reset the heap back to the start
void ResetHeap()
{
	mCurrentHeap = mHeap;
}

// initialise the heap
CMiniHeap(size_t size)
	: mHeap((char *)malloc(size)), mCurrentHeap(nullptr), mSize(size)
{
	if (mHeap)
	{
		ResetHeap();
	}
}

// free up the heap
~CMiniHeap()
{
	if (mHeap)
	{
		free(mHeap);
	}
}

// give me some space from the heap please
char *MiniHeapAlloc(size_t size)
{
	// no arithmetic on a NULL heap when malloc failed
	if (mHeap && size <= (size_t)(mHeap + mSize - mCurrentHeap))
	{
		char *tempAddress =  mCurrentHeap;
		mCurrentHeap += size;
		return tempAddress;
	}
	return nullptr;
}

};

extern CMiniHeap *G2VertSpaceServer;
extern CMiniHeap *G2VertSpaceClient;


#endif	//MINIHEAP_H_INC
