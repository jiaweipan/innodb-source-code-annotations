/******************************************************
The transaction lock system

(c) 1996 Innobase Oy

Created 5/7/1996 Heikki Tuuri
*******************************************************/
/*事务锁系统*/
#include "lock0lock.h"

#ifdef UNIV_NONINL
#include "lock0lock.ic"
#endif

#include "usr0sess.h"
#include "trx0purge.h"

/* Restricts the length of search we will do in the waits-for
graph of transactions */ /*限制在事务的等待图中进行的搜索长度*/
#define LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK 1000000

/* When releasing transaction locks, this specifies how often we release
the kernel mutex for a moment to give also others access to it */
/*当释放事务锁时，这指定了我们释放内核互斥锁的频率，以便让其他人也可以访问它*/
#define LOCK_RELEASE_KERNEL_INTERVAL	1000

/* Safety margin when creating a new record lock: this many extra records
can be inserted to the page without need to create a lock with a bigger
bitmap */
/*创建一个新的记录锁时的安全边距:这些额外的记录可以插入到页面中，而不需要创建一个更大的位图锁*/
#define LOCK_PAGE_BITMAP_MARGIN		64

/* An explicit record lock affects both the record and the gap before it.
An implicit x-lock does not affect the gap, it only locks the index
record from read or update. 
显式记录锁同时影响记录和它之前的间隙。隐式的x-lock不影响gap，它只锁定从读或更新的索引记录。
If a transaction has modified or inserted an index record, then
it owns an implicit x-lock on the record. On a secondary index record,
a transaction has an implicit x-lock also if it has modified the
clustered index record, the max trx id of the page where the secondary
index record resides is >= trx id of the transaction (or database recovery
is running), and there are no explicit non-gap lock requests on the
secondary index record.
如果一个事务修改或插入了一个索引记录，那么它对该记录拥有一个隐式x-锁。
二级索引记录,一个事务有一个隐式的独占锁如果修改了聚集索引记录,
二级索引记录所在页面的the max trx id > = trx的事务id(或数据库恢复运行),也没有明确的non-gap锁请求在二级索引记录。
This complicated definition for a secondary index comes from the
implementation: we want to be able to determine if a secondary index
record has an implicit x-lock, just by looking at the present clustered
index record, not at the historical versions of the record. The
complicated definition can be explained to the user so that there is
nondeterminism in the access path when a query is answered: we may,
or may not, access the clustered index record and thus may, or may not,
bump into an x-lock set there.
二级索引的这个复杂定义来自于实现:我们希望能够确定二级索引记录是否具有隐式x-lock，
只需查看当前聚集的索引记录，而不是该记录的历史版本。可以向用户解释复杂的定义，以便在回答查询时，
访问路径中存在不确定性:我们可能访问聚集索引记录，也可能不访问聚集索引记录，因此可能会在那里碰到x-lock集。
Different transaction can have conflicting locks set on the gap at the
same time. The locks on the gap are purely inhibitive: an insert cannot
be made, or a select cursor may have to wait, if a different transaction
has a conflicting lock on the gap. An x-lock on the gap does not give
the right to insert into the gap if there are conflicting locks granted
on the gap at the same time.
不同的事务可以同时在间隙上设置冲突锁。间隙上的锁是完全抑制的:如果不同的事务在间隙上有一个冲突的锁，
那么就不能进行插入，或者选择游标可能不得不等待。
如果在间隙上同时有冲突的锁，那么在间隙上的x锁不会给插入间隙的权利。
An explicit lock can be placed on a user record or the supremum record of
a page. The locks on the supremum record are always thought to be of the gap
type, though the gap bit is not set. When we perform an update of a record
where the size of the record changes, we may temporarily store its explicit
locks on the infimum record of the page, though the infimum otherwise never
carries locks.
显式锁可以放在用户记录或页面的最高记录上。上记录上的锁总是被认为是间隙类型的，尽管间隙位没有被设置。
当更新一条记录时，记录的大小发生了变化，我们可以将其显式锁临时存储在该页的下位记录上，尽管下位记录从不携带锁。
A waiting record lock can also be of the gap type. A waiting lock request
can be granted when there is no conflicting mode lock request by another
transaction ahead of it in the explicit lock queue.
等待记录锁也可以是间隙类型。当显式锁队列中前面的另一个事务没有冲突模式的锁请求时，可以授予一个等待的锁请求。
-------------------------------------------------------------------------
RULE 1: If there is an implicit x-lock on a record, and there are non-gap
lock requests waiting in the queue, then the transaction holding the implicit
x-lock also has an explicit non-gap record x-lock. Therefore, as locks are
released, we can grant locks to waiting lock requests purely by looking at
the explicit lock requests in the queue.
规则1:如果一个记录上有一个隐式的x-lock，并且队列中有非间隙锁请求在等待，
那么持有该隐式x-lock的事务也有一个显式的非间隙记录x-lock。因此，当锁被释放时，
我们可以通过查看队列中的显式锁请求，将锁授予等待的锁请求。
RULE 2: Granted non-gap locks on a record are always ahead in the queue
of waiting non-gap locks on a record.
规则2:一个记录上被授予的非间隙锁总是在一个记录上等待非间隙锁的队列中领先。
RULE 3: Different transactions cannot have conflicting granted non-gap locks
on a record at the same time. However, they can have conflicting granted gap
locks.
规则3:不同的事务不能同时在一个记录上有冲突的授予非间隙锁。但是，它们可能有冲突的授予间隙锁。
RULE 4: If a there is a waiting lock request in a queue, no lock request,
gap or not, can be inserted ahead of it in the queue. In record deletes
and page splits, new gap type locks can be created by the database manager
for a transaction, and without rule 4, the waits-for graph of transactions
might become cyclic without the database noticing it, as the deadlock check
is only performed when a transaction itself requests a lock!
规则4:如果队列中有一个正在等待的锁请求，无论是否有间隙，都不能将锁请求插入到该队列的前面。
在记录删除和页面分裂,可以创建新的缺口类型锁的事务数据库管理器,没有规则4,等待图的交易可能成为循环,
数据库未曾注意到的僵局检查只有当事务本身执行请求锁!
-------------------------------------------------------------------------

An insert is allowed to a gap if there are no explicit lock requests by
other transactions on the next record. It does not matter if these lock
requests are granted or waiting, gap bit set or not. On the other hand, an
implicit x-lock by another transaction does not prevent an insert, which
allows for more concurrency when using an Oracle-style sequence number
generator for the primary key with many transactions doing inserts
concurrently.
如果其他事务对下一个记录没有显式的锁请求，则允许插入间隔。
这些锁请求是被授予还是等待、间隙位是否设置都没有关系。
另一方面，另一个事务的隐式x-lock并不会阻止插入，因此当使用oracle风格的序列号生成器作为主键，
同时有许多事务同时执行插入时，就会允许更多的并发性。
A modify of a record is allowed if the transaction has an x-lock on the
record, or if other transactions do not have any non-gap lock requests on the
record.
如果事务对记录有x-lock，或者其他事务对记录没有任何非间隙锁请求，则允许对记录进行修改。
A read of a single user record with a cursor is allowed if the transaction
has a non-gap explicit, or an implicit lock on the record, or if the other
transactions have no x-lock requests on the record. At a page supremum a
read is always allowed.
如果事务对记录有非gap显式锁或隐式锁，或者其他事务对记录没有x-lock请求，
则允许使用游标读取单个用户记录。在页面上，读取总是被允许的。
In summary, an implicit lock is seen as a granted x-lock only on the
record, not on the gap. An explicit lock with no gap bit set is a lock
both on the record and the gap. If the gap bit is set, the lock is only
on the gap. Different transaction cannot own conflicting locks on the
record at the same time, but they may own conflicting locks on the gap.
Granted locks on a record give an access right to the record, but gap type
locks just inhibit operations.
总之，隐式锁只在记录上被视为授予的x-锁，而不是在间隙上。
没有间隙位设置的显式锁是记录和间隙上的锁。如果间隙位被设置，锁只在间隙上。
不同的事务不能同时拥有记录上的冲突锁，但它们可以拥有间隙上的冲突锁。
对记录授予的锁赋予了对记录的访问权，但是间隙类型的锁只是禁止操作。
NOTE: Finding out if some transaction has an implicit x-lock on a secondary
index record can be cumbersome. We may have to look at previous versions of
the corresponding clustered index record to find out if a delete marked
secondary index record was delete marked by an active transaction, not by
a committed one.
注意:查找某个事务是否在二级索引记录上有隐式x-锁可能很麻烦。
我们可能必须查看相应的聚集索引记录的以前版本，
以找出一个被删除标记的二级索引记录是否被一个活动事务删除，而不是被一个已提交的事务删除。
FACT A: If a transaction has inserted a row, it can delete it any time
without need to wait for locks.
事实A:如果一个事务插入了一行，它可以在任何时候删除它，而不需要等待锁。
PROOF: The transaction has an implicit x-lock on every index record inserted
for the row, and can thus modify each record without the need to wait. Q.E.D.
PROOF:事务对为该行插入的每条索引记录都有一个隐式的x-lock，因此可以修改每条记录，而不需要等待。Q.E.D.
FACT B: If a transaction has read some result set with a cursor, it can read
it again, and retrieves the same result set, if it has not modified the
result set in the meantime. Hence, there is no phantom problem. If the
biggest record, in the alphabetical order, touched by the cursor is removed,
a lock wait may occur, otherwise not.
事实B:如果事务使用游标读取了某个结果集，它可以再次读取该结果集，并检索相同的结果集(如果它在此期间没有修改结果集的话)。
因此，不存在幻像问题。如果游标所触摸的最大记录(按字母顺序)被删除，则可能发生锁等待，否则不会发生锁等待。
PROOF: When a read cursor proceeds, it sets an s-lock on each user record
it passes, and a gap type s-lock on each page supremum. The cursor must
wait until it has these locks granted. Then no other transaction can
have a granted x-lock on any of the user records, and therefore cannot
modify the user records. Neither can any other transaction insert into
the gaps which were passed over by the cursor. Page splits and merges,
and removal of obsolete versions of records do not affect this, because
when a user record or a page supremum is removed, the next record inherits
its locks as gap type locks, and therefore blocks inserts to the same gap.
Also, if a page supremum is inserted, it inherits its locks from the successor
record. When the cursor is positioned again at the start of the result set,
the records it will touch on its course are either records it touched
during the last pass or new inserted page supremums. It can immediately
access all these records, and when it arrives at the biggest record, it
notices that the result set is complete. If the biggest record was removed,
lock wait can occur because the next record only inherits a gap type lock,
and a wait may be needed. Q.E.D. 
证明:当读取指针继续时，它会对它所传递的每个用户记录设置一个s-lock，对每个页面上限值设置一个gap类型的s-lock。
游标必须等待，直到授予这些锁。那么任何其他事务都不能对任何用户记录授予x-lock，因此不能修改用户记录。
任何其他事务也不能插入由游标传递的间隙中。页面拆分和合并，以及删除过时版本的记录不会影响这一点，
因为当一个用户记录或一个页面上限被删除时，下一个记录将继承它的锁作为间隙类型锁，因此block插入到相同的间隙。
此外，如果插入了一个页面上限，它将从后续记录继承它的锁。当游标再次定位到结果集的开始位置时，
它将在其路径上触摸的记录要么是它在上次传递期间触摸的记录，要么是新插入的页面上限。
它可以立即访问所有这些记录，当它到达最大的记录时，它会注意到结果集已经完成。
如果最大的记录被删除，就会发生锁等待，因为下一个记录只继承一个gap类型的锁，可能需要等待。Q.E.D.*/
/* If an index record should be changed or a new inserted, we must check
the lock on the record or the next. When a read cursor starts reading,
we will set a record level s-lock on each record it passes, except on the
initial record on which the cursor is positioned before we start to fetch
records. Our index tree search has the convention that the B-tree
cursor is positioned BEFORE the first possibly matching record in
the search. Optimizations are possible here: if the record is searched
on an equality condition to a unique key, we could actually set a special
lock on the record, a lock which would not prevent any insert before
this record. In the next key locking an x-lock set on a record also
prevents inserts just before that record.
如果一个索引记录需要更改或新插入，则必须检查该记录或下一个记录上的锁。
当读取游标开始读取时，我们将对它所通过的每条记录设置一个记录级别s-lock，
除了在我们开始获取记录之前游标所定位的初始记录。我们的索引树搜索约定b -树游标定位在搜索中第一个可能匹配的记录之前。
这里有可能进行优化:如果记录是在一个唯一键的相等条件下搜索的，我们实际上可以在记录上设置一个特殊的锁，
这个锁不会阻止任何在这个记录之前的插入。在下一个键锁定中，记录上的x-lock集合也可以防止在该记录之前进行插入。
	There are special infimum and supremum records on each page.
A supremum record can be locked by a read cursor. This records cannot be
updated but the lock prevents insert of a user record to the end of
the page.
每一页都有特殊的下位和上位记录。一个上限记录可以被一个读游标锁定。此记录不能更新，但锁阻止将用户记录插入到页面的末尾。
	Next key locks will prevent the phantom problem where new rows
could appear to SELECT result sets after the select operation has been
performed. Prevention of phantoms ensures the serilizability of
transactions.
Next键锁将防止在执行选择操作后，新行可能出现在SELECT结果集中的幻像问题。防止幻像确保了事务的可串行化。
	What should we check if an insert of a new record is wanted?
Only the lock on the next record on the same page, because also the
supremum record can carry a lock. An s-lock prevents insertion, but
what about an x-lock? If it was set by a searched update, then there
is implicitly an s-lock, too, and the insert should be prevented.
What if our transaction owns an x-lock to the next record, but there is
a waiting s-lock request on the next record? If this s-lock was placed
by a read cursor moving in the ascending order in the index, we cannot
do the insert immediately, because when we finally commit our transaction,
the read cursor should see also the new inserted record. So we should
move the read cursor backward from the the next record for it to pass over
the new inserted record. This move backward may be too cumbersome to
implement. If we in this situation just enqueue a second x-lock request
for our transaction on the next record, then the deadlock mechanism
notices a deadlock between our transaction and the s-lock request
transaction. This seems to be an ok solution.
如果需要插入新记录，我们应该检查什么?只有锁在同一页上的下一个记录上，因为上记录也可以携带一个锁。
s-lock可以防止插入，但是x-lock呢?如果它是通过搜索更新设置的，那么也会隐式地有一个s-lock，应该防止插入。
如果我们的事务拥有下一个记录的x-lock，但是下一个记录上有一个等待s-lock请求，
该怎么办?如果这个s-lock是由一个在索引中按升序移动的读游标放置的，那么我们不能立即执行插入操作，
因为当我们最终提交事务时，读游标也应该看到新插入的记录。因此，我们应该将read游标从下一个记录向后移动，
以便它传递新插入的记录。这种向后移动可能太麻烦而难以实现。如果在这种情况下，我们只是在下一个记录上为我们的事务排队第二个x-lock请求，
那么死锁机制会注意到我们的事务和s-lock请求事务之间的死锁。这似乎是一个不错的解决方案。
	We could have the convention that granted explicit record locks,
lock the corresponding records from changing, and also lock the gaps
before them from inserting. A waiting explicit lock request locks the gap
before from inserting. Implicit record x-locks, which we derive from the
transaction id in the clustered index record, only lock the record itself
from modification, not the gap before it from inserting.
我们可以有这样的约定，即授予显式的记录锁，从更改中锁定相应的记录，并从插入中锁定它们之前的间隙。
一个等待的显式锁请求在插入之前锁定间隙。隐式记录x-locks(我们从聚集索引记录中的事务id派生)仅通过修改锁定记录本身，
而不是通过插入之前的间隙锁定记录。
	How should we store update locks? If the search is done by a unique
key, we could just modify the record trx id. Otherwise, we could put a record
x-lock on the record. If the update changes ordering fields of the
clustered index record, the inserted new record needs no record lock in
lock table, the trx id is enough. The same holds for a secondary index
record. Searched delete is similar to update.
我们应该如何存储更新锁?如果搜索是由一个唯一的键完成的，我们可以只修改记录trx id。
否则，我们可以在记录上加一个记录x锁。如果更新改变了聚集索引记录的排序字段，插入的新记录不需要锁表中的记录锁，
trx id就足够了。对于二级索引记录也是如此。搜索删除类似于更新。
PROBLEM:
What about waiting lock requests? If a transaction is waiting to make an
update to a record which another modified, how does the other transaction
know to send the end-lock-wait signal to the waiting transaction? If we have
the convention that a transaction may wait for just one lock at a time, how
do we preserve it if lock wait ends?
等待锁请求呢?如果一个事务正在等待对另一个事务修改的记录进行更新，
那么另一个事务如何知道向等待的事务发送end-lock-wait信号呢?
如果我们约定一个事务一次只等待一个锁，那么如果锁等待结束，我们如何保持它
PROBLEM:
Checking the trx id label of a secondary index record. In the case of a
modification, not an insert, is this necessary? A secondary index record
is modified only by setting or resetting its deleted flag. A secondary index
record contains fields to uniquely determine the corresponding clustered
index record. A secondary index record is therefore only modified if we
also modify the clustered index record, and the trx id checking is done
on the clustered index record, before we come to modify the secondary index
record. So, in the case of delete marking or unmarking a secondary index
record, we do not have to care about trx ids, only the locks in the lock
table must be checked. In the case of a select from a secondary index, the
trx id is relevant, and in this case we may have to search the clustered
index record.
检查二级索引记录的trx id标签。在修改的情况下，而不是插入，这是必要的吗?
二级索引记录只能通过设置或重置其已删除标志来修改。次要索引记录包含用于惟一地确定相应聚集索引记录的字段。
因此，只有当我们也修改了聚集索引记录时，辅助索引记录才会被修改，并且在修改辅助索引记录之前，对聚集索引记录进行trx id检查。
因此，在删除标记或不标记二级索引记录的情况下，我们不必关心trx id，只需要检查锁表中的锁。
对于从二级索引进行选择的情况，trx id是相关的，在这种情况下，我们可能必须搜索聚集索引记录。
PROBLEM: How to update record locks when page is split or merged, or
a record is deleted or updated?
If the size of fields in a record changes, we perform the update by
a delete followed by an insert. How can we retain the locks set or
waiting on the record? Because a record lock is indexed in the bitmap
by the heap number of the record, when we remove the record from the
record list, it is possible still to keep the lock bits. If the page
is reorganized, we could make a table of old and new heap numbers,
and permute the bitmaps in the locks accordingly. We can add to the
table a row telling where the updated record ended. If the update does
not require a reorganization of the page, we can simply move the lock
bits for the updated record to the position determined by its new heap
number (we may have to allocate a new lock, if we run out of the bitmap
in the old one).
	A more complicated case is the one where the reinsertion of the
updated record is done pessimistically, because the structure of the
tree may change.
问题:当页面被分割或合并，或记录被删除或更新时，如何更新记录锁?
如果记录中字段的大小发生变化，我们将通过删除和插入来执行更新。我们如何保持锁集或等待记录?
因为记录锁在位图中是根据记录的堆号索引的，所以当我们从记录列表中删除记录时，仍然可以保留锁位。
如果重新组织页，我们可以创建一个包含新旧堆数的表，并相应地排列锁中的位图。
我们可以向表中添加一行，说明更新记录的结束位置。
如果更新不需要重组的页面,我们可以简单地将锁位更新记录的位置决定了它的新堆数量(我们可能需要分配一个新的锁,如果我们用完旧的位图)。
更复杂的情况是，重新插入更新后的记录是悲观的，因为树的结构可能会改变。
PROBLEM: If a supremum record is removed in a page merge, or a record
---------------------------------------------------------------------
removed in a purge, what to do to the waiting lock requests? In a split to
the right, we just move the lock requests to the new supremum. If a record
is removed, we could move the waiting lock request to its inheritor, the
next record in the index. But, the next record may already have lock
requests on its own queue. A new deadlock check should be made then. Maybe
it is easier just to release the waiting transactions. They can then enqueue
new lock requests on appropriate records.
问题:如果一个上记录在页面合并中被删除，或者一个记录在清除中被删除，该如何处理等待的锁请求?
在右侧的拆分中，我们只是将锁请求移动到新的上界。如果删除了一条记录，我们可以将等待的锁请求移动到它的继承者，即索引中的下一条记录。
但是，下一个记录可能在它自己的队列上已经有了锁请求。然后应该进行一个新的死锁检查。也许释放等待的事务更容易。
然后，它们可以在适当的记录上对新锁请求进行排队。
PROBLEM: When a record is inserted, what locks should it inherit from the
-------------------------------------------------------------------------
upper neighbor? An insert of a new supremum record in a page split is
always possible, but an insert of a new user record requires that the upper
neighbor does not have any lock requests by other transactions, granted or
waiting, in its lock queue. Solution: We can copy the locks as gap type
locks, so that also the waiting locks are transformed to granted gap type
locks on the inserted record. 
问题:当一个记录被插入时，它应该从上邻居继承什么锁?在分页中插入一条新的上邻居记录总是可能的，
但是插入一条新的用户记录要求上邻居的锁队列中没有任何其他事务的锁请求，无论是被授予的还是正在等待的。
解决方案:我们可以将锁复制为间隙类型锁，这样等待的锁也会被转换为插入记录上的间隙类型锁。*/

ibool	lock_print_waits	= FALSE;

/* The lock system */
lock_sys_t*	lock_sys	= NULL;

/* A table lock */
typedef struct lock_table_struct	lock_table_t;
struct lock_table_struct{
	dict_table_t*	table;	/* database table in dictionary cache */
	UT_LIST_NODE_T(lock_t)
			locks; 	/* list of locks on the same table */
};

/* Record lock for a page */
typedef struct lock_rec_struct		lock_rec_t;
struct lock_rec_struct{
	ulint	space;		/* space id */
	ulint	page_no;	/* page number */
	ulint	n_bits;		/* number of bits in the lock bitmap */
				/* NOTE: the lock bitmap is placed immediately
				after the lock struct */
};

/* Lock struct */
struct lock_struct{
	trx_t*		trx;		/* transaction owning the lock */
	UT_LIST_NODE_T(lock_t)		
			trx_locks;	/* list of the locks of the
					transaction */
	ulint		type_mode;	/* lock type, mode, gap flag, and
					wait flag, ORed */
	hash_node_t	hash;		/* hash chain node for a record lock */
	dict_index_t*	index;		/* index for a record lock */
	union {
		lock_table_t	tab_lock;/* table lock */
		lock_rec_t	rec_lock;/* record lock */
	} un_member;
};

/************************************************************************
Checks if a lock request results in a deadlock. */ /*检查锁请求是否导致死锁。*/
static
ibool
lock_deadlock_occurs(
/*=================*/
			/* out: TRUE if a deadlock was detected */
	lock_t*	lock,	/* in: lock the transaction is requesting */
	trx_t*	trx);	/* in: transaction */
/************************************************************************
Looks recursively for a deadlock. */ /*递归查找死锁。*/
static
ibool
lock_deadlock_recursive(
/*====================*/
				/* out: TRUE if a deadlock was detected
				or the calculation took too long */
	trx_t*	start,		/* in: recursion starting point */
	trx_t*	trx,		/* in: a transaction waiting for a lock */
	lock_t*	wait_lock,	/* in: the lock trx is waiting to be granted */
	ulint*	cost);		/* in/out: number of calculation steps thus
				far: if this exceeds LOCK_MAX_N_STEPS_...
				we return TRUE */
/*************************************************************************
Reserves the kernel mutex. This function is used in this module to allow
monitoring the contention degree on the kernel mutex caused by the lock
operations. *//*保留内核互斥。这个函数用于监控锁操作引起的内核互斥锁的争用程度。*/
UNIV_INLINE
void
lock_mutex_enter_kernel(void)
/*=========================*/
{
	mutex_enter(&kernel_mutex);
}

/*************************************************************************
Releses the kernel mutex. This function is used in this module to allow
monitoring the contention degree on the kernel mutex caused by the lock
operations. */ /*释放内核互斥锁。这个函数用于监控锁操作引起的内核互斥锁的争用程度。*/
UNIV_INLINE
void
lock_mutex_exit_kernel(void)
/*=========================*/
{
	mutex_exit(&kernel_mutex);
}

#ifdef notdefined

/*************************************************************************
Gets the mutex protecting record locks for a page in the buffer pool. */
/*获取保护缓冲池中某一页的记录锁的互斥锁。*/
UNIV_INLINE
mutex_t*
lock_rec_get_mutex(
/*===============*/
	byte*	ptr)	/* in: pointer to somewhere within a buffer frame */
{
	return(buf_frame_get_lock_mutex(ptr));
}
	
/*************************************************************************
Reserves the mutex protecting record locks for a page in the buffer pool. */
/*在缓冲池中为页面保留保护记录锁的互斥锁。*/
UNIV_INLINE
void
lock_rec_mutex_enter(
/*=================*/
	byte*	ptr)	/* in: pointer to somewhere within a buffer frame */
{
	mutex_enter(lock_rec_get_mutex(ptr));
}

/*************************************************************************
Releases the mutex protecting record locks for a page in the buffer pool. */
/*释放保护缓冲池中某个页面的记录锁的互斥锁。*/
UNIV_INLINE
void
lock_rec_mutex_exit(
/*================*/
	byte*	ptr)	/* in: pointer to somewhere within a buffer frame */
{
	mutex_exit(lock_rec_get_mutex(ptr));
}

/*************************************************************************
Checks if the caller owns the mutex to record locks of a page. Works only in
the debug version. */ /*检查调用者是否拥有记录页面锁的互斥锁。只能在调试版本中工作。*/
UNIV_INLINE
ibool
lock_rec_mutex_own(
/*===============*/
			/* out: TRUE if the current OS thread has reserved the
			mutex */
	byte*	ptr)	/* in: pointer to somewhere within a buffer frame */
{
	return(mutex_own(lock_rec_get_mutex(ptr)));
}

/*************************************************************************
Gets the mutex protecting record locks on a given page address. */
/*获取保护给定页面地址上的记录锁的互斥锁。*/
mutex_t*
lock_rec_get_mutex_for_addr(
/*========================*/
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	return(hash_get_mutex(lock_sys->rec_hash,
					lock_rec_fold(space, page_no)));
}

/*************************************************************************
Checks if the caller owns the mutex to record locks of a page. Works only in
the debug version. */ /*检查调用者是否拥有记录页面锁的互斥锁。只能在调试版本中工作。*/
UNIV_INLINE
ibool
lock_rec_mutex_own_addr(
/*====================*/
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	return(mutex_own(lock_rec_get_mutex_for_addr(space, page_no)));
}

/*************************************************************************
Reserves all the mutexes protecting record locks. */ /*保留保护记录锁的所有互斥锁。*/
UNIV_INLINE
void
lock_rec_mutex_enter_all(void)
/*==========================*/
{
	hash_table_t* 	table; 
	ulint		n_mutexes;
	ulint		i;

	table = lock_sys->rec_hash;

	n_mutexes = table->n_mutexes;

	for (i = 0; i < n_mutexes; i++) {

		mutex_enter(hash_get_nth_mutex(table, i));
	}
}

/*************************************************************************
Releases all the mutexes protecting record locks. */ /*释放保护记录锁的所有互斥锁。*/
UNIV_INLINE
void
lock_rec_mutex_exit_all(void)
/*=========================*/
{
	hash_table_t* 	table; 
	ulint		n_mutexes;
	ulint		i;

	table = lock_sys->rec_hash;

	n_mutexes = table->n_mutexes;

	for (i = 0; i < n_mutexes; i++) {

		mutex_exit(hash_get_nth_mutex(table, i));
	}
}

/*************************************************************************
Checks that the current OS thread owns all the mutexes protecting record
locks. */ /*检查当前操作系统线程是否拥有保护记录锁的所有互斥锁。*/
UNIV_INLINE
ibool
lock_rec_mutex_own_all(void)
/*========================*/
				/* out: TRUE if owns all */
{
	hash_table_t* 	table; 
	ulint		n_mutexes;
	ibool		owns_yes	= TRUE;
	ulint		i;

	table = lock_sys->rec_hash;

	n_mutexes = table->n_mutexes;

	for (i = 0; i < n_mutexes; i++) {
		if (!mutex_own(hash_get_nth_mutex(table, i))) {

			owns_yes = FALSE;
		}
	}

	return(owns_yes);
}

#endif

/*************************************************************************
Checks that a record is seen in a consistent read. */
/*检查是否在一致的读取中看到一条记录。*/
ibool
lock_clust_rec_cons_read_sees(
/*==========================*/
				/* out: TRUE if sees, or FALSE if an earlier
				version of the record should be retrieved */
	rec_t*		rec,	/* in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/* in: clustered index */
	read_view_t*	view)	/* in: consistent read view */
{
	dulint	trx_id;

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(page_rec_is_user_rec(rec));

	trx_id = row_get_rec_trx_id(rec, index);
	
	if (read_view_sees_trx_id(view, trx_id)) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Checks that a non-clustered index record is seen in a consistent read. */
/*检查非聚集索引记录是否出现在一致的读取中。*/
ulint
lock_sec_rec_cons_read_sees(
/*========================*/
				/* out: TRUE if certainly sees, or FALSE if an
				earlier version of the clustered index record
				might be needed: NOTE that a non-clustered
				index page contains so little information on
				its modifications that also in the case FALSE,
				the present version of rec may be the right,
				but we must check this from the clustered
				index record */
	rec_t*		rec,	/* in: user record which should be read or
				passed over by a read cursor */
	dict_index_t*	index,	/* in: non-clustered index */
	read_view_t*	view)	/* in: consistent read view */
{
	dulint	max_trx_id;

	ut_ad(!(index->type & DICT_CLUSTERED));
	ut_ad(page_rec_is_user_rec(rec));

	if (recv_recovery_is_on()) {

		return(FALSE);
	}

	max_trx_id = page_get_max_trx_id(buf_frame_align(rec));

	if (ut_dulint_cmp(max_trx_id, view->up_limit_id) >= 0) {

		return(FALSE);
	}

	return(TRUE);
}

/*************************************************************************
Creates the lock system at database start. */
/*在数据库启动时创建锁系统。*/
void
lock_sys_create(
/*============*/
	ulint	n_cells)	/* in: number of slots in lock hash table */
{
	lock_sys = mem_alloc(sizeof(lock_sys_t));

	lock_sys->rec_hash = hash_create(n_cells);

	/* hash_create_mutexes(lock_sys->rec_hash, 2, SYNC_REC_LOCK); */
}

/*************************************************************************
Gets the size of a lock struct. */
/*获取锁结构体的大小。*/
ulint
lock_get_size(void)
/*===============*/
			/* out: size in bytes */
{
	return((ulint)sizeof(lock_t));
}

/*************************************************************************
Gets the mode of a lock. */ /*获取锁的模式。*/
UNIV_INLINE
ulint
lock_get_mode(
/*==========*/
			/* out: mode */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(lock);

	return(lock->type_mode & LOCK_MODE_MASK);
}

/*************************************************************************
Gets the type of a lock. */ /*获取锁的类型。*/
UNIV_INLINE
ulint
lock_get_type(
/*==========*/
			/* out: LOCK_TABLE or LOCK_RECa */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(lock);

	return(lock->type_mode & LOCK_TYPE_MASK);
}

/*************************************************************************
Gets the wait flag of a lock. */ /*获取锁的等待标志。*/
UNIV_INLINE
ibool
lock_get_wait(
/*==========*/
			/* out: TRUE if waiting */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(lock);

	if (lock->type_mode & LOCK_WAIT) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Sets the wait flag of a lock and the back pointer in trx to lock. */
/*设置锁的等待标志和trx中的返回指针来锁定。*/
UNIV_INLINE
void
lock_set_lock_and_trx_wait(
/*=======================*/
	lock_t*	lock,	/* in: lock */
	trx_t*	trx)	/* in: trx */
{
	ut_ad(lock);
	ut_ad(trx->wait_lock == NULL);
	
	trx->wait_lock = lock;
 	lock->type_mode = lock->type_mode | LOCK_WAIT;
}

/**************************************************************************
The back pointer to a waiting lock request in the transaction is set to NULL
and the wait bit in lock type_mode is reset. */
/*事务中等待锁请求的返回指针被设置为NULL，锁类型模式中的等待位被重置。*/
UNIV_INLINE
void
lock_reset_lock_and_trx_wait(
/*=========================*/
	lock_t*	lock)	/* in: record lock */
{
	ut_ad((lock->trx)->wait_lock == lock);
	ut_ad(lock_get_wait(lock));

	/* Reset the back pointer in trx to this waiting lock request */

	(lock->trx)->wait_lock = NULL;
 	lock->type_mode = lock->type_mode & ~LOCK_WAIT;
}

/*************************************************************************
Gets the gap flag of a record lock. */ /*获取记录锁的间隙标志。*/
UNIV_INLINE
ibool
lock_rec_get_gap(
/*=============*/
			/* out: TRUE if gap flag set */
	lock_t*	lock)	/* in: record lock */
{
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (lock->type_mode & LOCK_GAP) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Sets the gap flag of a record lock. */ /*设置记录锁的间隙标志。*/
UNIV_INLINE
void
lock_rec_set_gap(
/*=============*/
	lock_t*	lock,	/* in: record lock */
	ibool	val)	/* in: value to set: TRUE or FALSE */
{
	ut_ad(lock);
	ut_ad((val == TRUE) || (val == FALSE));
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (val) {
 		lock->type_mode = lock->type_mode | LOCK_GAP;
	} else {
		lock->type_mode = lock->type_mode & ~LOCK_GAP;
	}
}

/*************************************************************************
Calculates if lock mode 1 is stronger or equal to lock mode 2. */
/*计算锁定模式1是否更强或等于锁定模式2。*/
UNIV_INLINE
ibool
lock_mode_stronger_or_eq(
/*=====================*/
			/* out: TRUE if mode1 stronger or equal to mode2 */
	ulint	mode1,	/* in: lock mode */
	ulint	mode2)	/* in: lock mode */
{
	ut_ad(mode1 == LOCK_X || mode1 == LOCK_S || mode1 == LOCK_IX
				|| mode1 == LOCK_IS || mode1 == LOCK_AUTO_INC);
	ut_ad(mode2 == LOCK_X || mode2 == LOCK_S || mode2 == LOCK_IX
				|| mode2 == LOCK_IS || mode2 == LOCK_AUTO_INC);
	if (mode1 == LOCK_X) {

		return(TRUE);

	} else if (mode1 == LOCK_AUTO_INC && mode2 == LOCK_AUTO_INC) {

		return(TRUE);

	} else if (mode1 == LOCK_S
				&& (mode2 == LOCK_S || mode2 == LOCK_IS)) {
		return(TRUE);

	} else if (mode1 == LOCK_IS && mode2 == LOCK_IS) {

		return(TRUE);

	} else if (mode1 == LOCK_IX && (mode2 == LOCK_IX
						|| mode2 == LOCK_IS)) {
		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Calculates if lock mode 1 is compatible with lock mode 2. */ /*计算锁模式1是否与锁模式2兼容。*/
UNIV_INLINE
ibool
lock_mode_compatible(
/*=================*/
			/* out: TRUE if mode1 compatible with mode2 */
	ulint	mode1,	/* in: lock mode */
	ulint	mode2)	/* in: lock mode */
{
	ut_ad(mode1 == LOCK_X || mode1 == LOCK_S || mode1 == LOCK_IX
				|| mode1 == LOCK_IS || mode1 == LOCK_AUTO_INC);
	ut_ad(mode2 == LOCK_X || mode2 == LOCK_S || mode2 == LOCK_IX
				|| mode2 == LOCK_IS || mode2 == LOCK_AUTO_INC);

	if (mode1 == LOCK_S && (mode2 == LOCK_IS || mode2 == LOCK_S)) {

		return(TRUE);

	} else if (mode1 == LOCK_X) {

		return(FALSE);

	} else if (mode1 == LOCK_AUTO_INC && (mode2 == LOCK_IS
					  	|| mode2 == LOCK_IX)) {
		return(TRUE);

	} else if (mode1 == LOCK_IS && (mode2 == LOCK_IS
					  	|| mode2 == LOCK_IX
					  	|| mode2 == LOCK_AUTO_INC
					  	|| mode2 == LOCK_S)) {
		return(TRUE);

	} else if (mode1 == LOCK_IX && (mode2 == LOCK_IS
					  	|| mode2 == LOCK_AUTO_INC
					  	|| mode2 == LOCK_IX)) {
		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Returns LOCK_X if mode is LOCK_S, and vice versa. */ /*如果模式为LOCK_S，则返回LOCK_X，反之亦然。*/
UNIV_INLINE
ulint
lock_get_confl_mode(
/*================*/
			/* out: conflicting basic lock mode */
	ulint	mode)	/* in: LOCK_S or LOCK_X */
{
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	if (mode == LOCK_S) {

		return(LOCK_X);
	}

	return(LOCK_S);
}

/*************************************************************************
Checks if a lock request lock1 has to wait for request lock2. NOTE that we,
for simplicity, ignore the gap bits in locks, and treat gap type lock
requests like non-gap lock requests. */ 
/*检查锁请求lock1是否必须等待请求lock2。注意，为了简单起见，我们忽略锁中的间隙位，而将间隙类型的锁请求视为非间隙锁请求。*/
UNIV_INLINE
ibool
lock_has_to_wait(
/*=============*/
			/* out: TRUE if lock1 has to wait lock2 to be removed */
	lock_t*	lock1,	/* in: waiting record lock */
	lock_t*	lock2)	/* in: another lock; NOTE that it is assumed that this
			has a lock bit set on the same record as in lock1 */
{
	if (lock1->trx != lock2->trx
			&& !lock_mode_compatible(lock_get_mode(lock1),
				     		lock_get_mode(lock2))) {
		return(TRUE);
	}

	return(FALSE);
}

/*============== RECORD LOCK BASIC FUNCTIONS ============================*/

/*************************************************************************
Gets the number of bits in a record lock bitmap. */ /*获取记录锁定位图中的位数。*/
UNIV_INLINE
ulint
lock_rec_get_n_bits(
/*================*/
			/* out: number of bits */
	lock_t*	lock)	/* in: record lock */
{
	return(lock->un_member.rec_lock.n_bits);
}

/*************************************************************************
Gets the nth bit of a record lock. */ /*获取记录锁的第n位。*/
UNIV_INLINE
ibool
lock_rec_get_nth_bit(
/*=================*/
			/* out: TRUE if bit set */
	lock_t*	lock,	/* in: record lock */
	ulint	i)	/* in: index of the bit */
{
	ulint	byte_index;
	ulint	bit_index;
	ulint	b;

	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);

	if (i >= lock->un_member.rec_lock.n_bits) {

		return(FALSE);
	}

	byte_index = i / 8;
	bit_index = i % 8;

	b = (ulint)*((byte*)lock + sizeof(lock_t) + byte_index);

	return(ut_bit_get_nth(b, bit_index));
}	

/**************************************************************************
Sets the nth bit of a record lock to TRUE. */ /*设置记录锁的第n位。*/
UNIV_INLINE
void
lock_rec_set_nth_bit(
/*==================*/
	lock_t*	lock,	/* in: record lock */
	ulint	i)	/* in: index of the bit */
{
	ulint	byte_index;
	ulint	bit_index;
	byte*	ptr;
	ulint	b;
	
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);
	ut_ad(i < lock->un_member.rec_lock.n_bits);
	
	byte_index = i / 8;
	bit_index = i % 8;

	ptr = (byte*)lock + sizeof(lock_t) + byte_index;
		
	b = (ulint)*ptr;

	b = ut_bit_set_nth(b, bit_index, TRUE);

	*ptr = (byte)b;
}	

/**************************************************************************
Looks for a set bit in a record lock bitmap. Returns ULINT_UNDEFINED,
if none found. */ /*在记录锁位图中查找设置位。如果没有找到，返回ULINT_UNDEFINED。*/
static
ulint
lock_rec_find_set_bit(
/*==================*/
			/* out: bit index == heap number of the record, or
			ULINT_UNDEFINED if none found */
	lock_t*	lock)	/* in: record lock with at least one bit set */
{
	ulint	i;

	for (i = 0; i < lock_rec_get_n_bits(lock); i++) {

		if (lock_rec_get_nth_bit(lock, i)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**************************************************************************
Resets the nth bit of a record lock. */
/*重置记录锁定的第n位。*/
UNIV_INLINE
void
lock_rec_reset_nth_bit(
/*===================*/
	lock_t*	lock,	/* in: record lock */
	ulint	i)	/* in: index of the bit which must be set to TRUE
			when this function is called */
{
	ulint	byte_index;
	ulint	bit_index;
	byte*	ptr;
	ulint	b;
	
	ut_ad(lock);
	ut_ad(lock_get_type(lock) == LOCK_REC);
	ut_ad(i < lock->un_member.rec_lock.n_bits);
	
	byte_index = i / 8;
	bit_index = i % 8;

	ptr = (byte*)lock + sizeof(lock_t) + byte_index;
		
	b = (ulint)*ptr;

	b = ut_bit_set_nth(b, bit_index, FALSE);

	*ptr = (byte)b;
}	

/*************************************************************************
Gets the first or next record lock on a page. */ /*获取页上的第一个或下一个记录锁定。*/
UNIV_INLINE
lock_t*
lock_rec_get_next_on_page(
/*======================*/
			/* out: next lock, NULL if none exists */
	lock_t*	lock)	/* in: a record lock */
{
	ulint	space;
	ulint	page_no;

	ut_ad(mutex_own(&kernel_mutex));

	space = lock->un_member.rec_lock.space;
	page_no = lock->un_member.rec_lock.page_no;
	
	for (;;) {
		lock = HASH_GET_NEXT(hash, lock);

		if (!lock) {

			break;
		}

		if ((lock->un_member.rec_lock.space == space) 
	    	    && (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}
	}
	
	return(lock);
}

/*************************************************************************
Gets the first record lock on a page, where the page is identified by its
file address. *//*获取页上的第一个记录锁定，其中页由其文件地址标识。*/
UNIV_INLINE
lock_t*
lock_rec_get_first_on_page_addr(
/*============================*/
			/* out: first lock, NULL if none exists */
	ulint	space,	/* in: space */
	ulint	page_no)/* in: page number */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = HASH_GET_FIRST(lock_sys->rec_hash,
					lock_rec_hash(space, page_no));
	while (lock) {
		if ((lock->un_member.rec_lock.space == space) 
	    	    && (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}

		lock = HASH_GET_NEXT(hash, lock);
	}

	return(lock);
}
	
/*************************************************************************
Returns TRUE if there are explicit record locks on a page. */
/*如果页面上有显式记录锁，则返回TRUE。*/
ibool
lock_rec_expl_exist_on_page(
/*========================*/
			/* out: TRUE if there are explicit record locks on
			the page */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	ibool	ret;

	mutex_enter(&kernel_mutex);

	if (lock_rec_get_first_on_page_addr(space, page_no)) {
		ret = TRUE;
	} else {
		ret = FALSE;
	}

	mutex_exit(&kernel_mutex);
	
	return(ret);
}

/*************************************************************************
Gets the first record lock on a page, where the page is identified by a
pointer to it. */ /*获取页上的第一个记录锁定，其中页由指向它的指针标识。*/
UNIV_INLINE
lock_t*
lock_rec_get_first_on_page(
/*=======================*/
			/* out: first lock, NULL if none exists */
	byte*	ptr)	/* in: pointer to somewhere on the page */
{
	ulint	hash;
	lock_t*	lock;
	ulint	space;
	ulint	page_no;

	ut_ad(mutex_own(&kernel_mutex));
	
	hash = buf_frame_get_lock_hash_val(ptr);

	lock = HASH_GET_FIRST(lock_sys->rec_hash, hash);

	while (lock) {
		space = buf_frame_get_space_id(ptr);
		page_no = buf_frame_get_page_no(ptr);

		if ((lock->un_member.rec_lock.space == space) 
	    		&& (lock->un_member.rec_lock.page_no == page_no)) {

			break;
		}

		lock = HASH_GET_NEXT(hash, lock);
	}

	return(lock);
}

/*************************************************************************
Gets the next explicit lock request on a record. */ /*获取记录上的下一个显式锁请求。*/
UNIV_INLINE
lock_t*
lock_rec_get_next(
/*==============*/
			/* out: next lock, NULL if none exists */
	rec_t*	rec,	/* in: record on a page */
	lock_t*	lock)	/* in: lock */
{
	ut_ad(mutex_own(&kernel_mutex));

	for (;;) {
		lock = lock_rec_get_next_on_page(lock);

		if (lock == NULL) {

			return(NULL);
		}

		if (lock_rec_get_nth_bit(lock, rec_get_heap_no(rec))) {

			return(lock);
		}
	}
}

/*************************************************************************
Gets the first explicit lock request on a record. */ /*获取记录上的第一个显式锁请求。*/
UNIV_INLINE
lock_t*
lock_rec_get_first(
/*===============*/
			/* out: first lock, NULL if none exists */
	rec_t*	rec)	/* in: record on a page */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = lock_rec_get_first_on_page(rec);

	while (lock) {
		if (lock_rec_get_nth_bit(lock, rec_get_heap_no(rec))) {

			break;
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	return(lock);
}

/*************************************************************************
Resets the record lock bitmap to zero. NOTE: does not touch the wait_lock
pointer in the transaction! This function is used in lock object creation
and resetting. */
/*将记录锁定位图重置为零。注意:不接触事务中的wait_lock指针!此函数用于创建和重置锁对象。*/
static
void
lock_rec_bitmap_reset(
/*==================*/
	lock_t*	lock)	/* in: record lock */
{
	byte*	ptr;
	ulint	n_bytes;
	ulint	i;

	/* Reset to zero the bitmap which resides immediately after the lock
	struct */ /*将紧挨着锁结构体的位图重置为零*/

	ptr = (byte*)lock + sizeof(lock_t);

	n_bytes = lock_rec_get_n_bits(lock) / 8;

	ut_ad((lock_rec_get_n_bits(lock) % 8) == 0);
	
	for (i = 0; i < n_bytes; i++) {

		*ptr = 0;
		ptr++;
	}
}

/*************************************************************************
Copies a record lock to heap. */ /*将记录锁复制到堆。*/
static
lock_t*
lock_rec_copy(
/*==========*/
				/* out: copy of lock */
	lock_t*		lock,	/* in: record lock */
	mem_heap_t*	heap)	/* in: memory heap */
{
	lock_t*	dupl_lock;
	ulint	size;

	size = sizeof(lock_t) + lock_rec_get_n_bits(lock) / 8;	

	dupl_lock = mem_heap_alloc(heap, size);

	ut_memcpy(dupl_lock, lock, size);

	return(dupl_lock);
}

/*************************************************************************
Gets the previous record lock set on a record. */ /*获取在记录上设置的前一个记录锁。*/
static
lock_t*
lock_rec_get_prev(
/*==============*/
			/* out: previous lock on the same record, NULL if
			none exists */
	lock_t*	in_lock,/* in: record lock */
	ulint	heap_no)/* in: heap number of the record */
{
	lock_t*	lock;
	ulint	space;
	ulint	page_no;
	lock_t*	found_lock 	= NULL;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type(in_lock) == LOCK_REC);

	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	for (;;) {
		ut_ad(lock);
		
		if (lock == in_lock) {

			return(found_lock);
		}

		if (lock_rec_get_nth_bit(lock, heap_no)) {

			found_lock = lock;
		}

		lock = lock_rec_get_next_on_page(lock);
	}	
}

/*============= FUNCTIONS FOR ANALYZING TABLE LOCK QUEUE ================*/
/*用于分析表锁队列的函数*/
/*************************************************************************
Checks if a transaction has the specified table lock, or stronger. */
/*检查事务是否具有指定的表锁或更强的锁。*/
UNIV_INLINE
lock_t*
lock_table_has(
/*===========*/
				/* out: lock or NULL */
	trx_t*		trx,	/* in: transaction */
	dict_table_t*	table,	/* in: table */
	ulint		mode)	/* in: lock mode */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	/* Look for stronger locks the same trx already has on the table */
    /* 寻找更强的锁相同的trx已经在表上*/
	lock = UT_LIST_GET_LAST(table->locks);

	while (lock != NULL) {

		if (lock->trx == trx
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)) {

			/* The same trx already has locked the table in 
			a mode stronger or equal to the mode given */
            /*相同的trx已经以更强或等于给定模式的模式锁定了表*/
			ut_ad(!lock_get_wait(lock)); 

			return(lock);
		}

		lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
	}

	return(NULL);
}
	
/*============= FUNCTIONS FOR ANALYZING RECORD LOCK QUEUE ================*/

/*************************************************************************
Checks if a transaction has a GRANTED explicit non-gap lock on rec, stronger
or equal to mode. */ /*检查一个事务是否在rec上有一个GRANTED显式非间隙锁，更强或等于mode。*/
UNIV_INLINE
lock_t*
lock_rec_has_expl(
/*==============*/
			/* out: lock or NULL */
	ulint	mode,	/* in: lock mode */
	rec_t*	rec,	/* in: record */
	trx_t*	trx)	/* in: transaction */
{
	lock_t*	lock;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((mode == LOCK_X) || (mode == LOCK_S));

	lock = lock_rec_get_first(rec);

	while (lock) {
		if (lock->trx == trx
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)
		    && !lock_get_wait(lock)
		    && !(lock_rec_get_gap(lock)
					|| page_rec_is_supremum(rec))) {
		    	return(lock);
		}

		lock = lock_rec_get_next(rec, lock);
	}

	return(NULL);
}
			
/*************************************************************************
Checks if some other transaction has an explicit lock request stronger or
equal to mode on rec or gap, waiting or granted, in the lock queue. */
/*检查是否有其他事务的锁请求更强或等于模式上的rec或gap，等待或批准，在锁队列中。*/
UNIV_INLINE
lock_t*
lock_rec_other_has_expl_req(
/*========================*/
			/* out: lock or NULL */
	ulint	mode,	/* in: lock mode */
	ulint	gap,	/* in: LOCK_GAP if also gap locks are taken
			into account, or 0 if not */
	ulint	wait,	/* in: LOCK_WAIT if also waiting locks are
			taken into account, or 0 if not */
	rec_t*	rec,	/* in: record to look at */	
	trx_t*	trx)	/* in: transaction, or NULL if requests
			by any transaction are wanted */
{
	lock_t*	lock;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((mode == LOCK_X) || (mode == LOCK_S));

	lock = lock_rec_get_first(rec);

	while (lock) {
		if (lock->trx != trx
		    && (gap ||
			!(lock_rec_get_gap(lock) || page_rec_is_supremum(rec)))
		    && (wait || !lock_get_wait(lock))
		    && lock_mode_stronger_or_eq(lock_get_mode(lock), mode)) {

		    	return(lock);
		}

		lock = lock_rec_get_next(rec, lock);
	}

	return(NULL);
}

/*************************************************************************
Looks for a suitable type record lock struct by the same trx on the same page.
This can be used to save space when a new record lock should be set on a page:
no new struct is needed, if a suitable old is found. */
/*在同一页面上通过相同的trx寻找合适的类型记录锁结构体。当一个新的记录锁被设置在一个页面上时，
这可以用来节省空间:如果找到一个合适的旧结构，则不需要新的结构。*/
UNIV_INLINE
lock_t*
lock_rec_find_similar_on_page(
/*==========================*/
				/* out: lock or NULL */
	ulint	type_mode,	/* in: lock type_mode field */
	rec_t*	rec,		/* in: record */
	trx_t*	trx)		/* in: transaction */
{
	lock_t*	lock;
	ulint	heap_no;

	ut_ad(mutex_own(&kernel_mutex));

	heap_no = rec_get_heap_no(rec);
	
	lock = lock_rec_get_first_on_page(rec);

	while (lock != NULL) {
		if (lock->trx == trx
		    && lock->type_mode == type_mode
		    && lock_rec_get_n_bits(lock) > heap_no) {
		    	
			return(lock);
		}
		
		lock = lock_rec_get_next_on_page(lock);
	}

	return(NULL);
}

/*************************************************************************
Checks if some transaction has an implicit x-lock on a record in a secondary
index. */
/*检查某个事务是否对二级索引中的记录具有隐式x-锁。*/
trx_t*
lock_sec_rec_some_has_impl_off_kernel(
/*==================================*/
				/* out: transaction which has the x-lock, or
				NULL */
	rec_t*		rec,	/* in: user record */
	dict_index_t*	index)	/* in: secondary index */
{
	page_t*	page;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(!(index->type & DICT_CLUSTERED));
	ut_ad(page_rec_is_user_rec(rec));

	page = buf_frame_align(rec);

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list, or
	database recovery is running. We do not write the changes of a page
	max trx id to the log, and therefore during recovery, this value
	for a page may be incorrect. */
    /*只有当页面的最大trx id >= trx列表的最小trx id，或者数据库恢复正在运行时，
	某些事务可能在记录上有一个隐式的x-lock。我们不会将页面最大trx id的更改写入日志，
	因此在恢复期间，页面的这个值可能是不正确的。*/
	if (!(ut_dulint_cmp(page_get_max_trx_id(page),
					trx_list_get_min_trx_id()) >= 0)
	   		&& !recv_recovery_is_on()) {

		return(NULL);
	}

	/* Ok, in this case it is possible that some transaction has an
	implicit x-lock. We have to look in the clustered index. */
	/*在这种情况下，有可能某个事务有一个隐式的x-锁。我们得看看聚集索引。*/		
	return(row_vers_impl_x_locked_off_kernel(rec, index));
}

/*============== RECORD LOCK CREATION AND QUEUE MANAGEMENT =============*/

/*************************************************************************
Creates a new record lock and inserts it to the lock queue. Does NOT check
for deadlocks or lock compatibility! */ /*创建一个新的记录锁并将其插入到锁队列中。不检查死锁或锁兼容性!*/
static
lock_t*
lock_rec_create(
/*============*/
				/* out: created lock, NULL if out of memory */
	ulint		type_mode,/* in: lock mode and wait flag, type is
				ignored and replaced by LOCK_REC */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: index of record */
	trx_t*		trx)	/* in: transaction */
{
	page_t*	page;
	lock_t*	lock;
	ulint	page_no;
	ulint	heap_no;
	ulint	space;
	ulint	n_bits;
	ulint	n_bytes;
	
	ut_ad(mutex_own(&kernel_mutex));

	page = buf_frame_align(rec);
	space = buf_frame_get_space_id(page);
	page_no	= buf_frame_get_page_no(page);
	heap_no = rec_get_heap_no(rec);

	/* If rec is the supremum record, then we reset the gap bit, as
	all locks on the supremum are automatically of the gap type, and
	we try to avoid unnecessary memory consumption of a new record lock
	struct for a gap type lock */
    /*如果rec是上记录，那么我们重置gap位，因为上记录上的所有锁都自动是gap类型的，并且我们试图避免对gap类型锁的新记录锁结构的不必要的内存消耗*/
	if (rec == page_get_supremum_rec(page)) {

		type_mode = type_mode & ~LOCK_GAP;
	}

	/* Make lock bitmap bigger by a safety margin */ /*使锁位图在安全裕度上变大*/
	n_bits = page_header_get_field(page, PAGE_N_HEAP)
						+ LOCK_PAGE_BITMAP_MARGIN;
	n_bytes = 1 + n_bits / 8;

	lock = mem_heap_alloc(trx->lock_heap, sizeof(lock_t) + n_bytes);
	
	if (lock == NULL) {

		return(NULL);
	}

	UT_LIST_ADD_LAST(trx_locks, trx->trx_locks, lock);

	lock->trx = trx;

	lock->type_mode = (type_mode & ~LOCK_TYPE_MASK) | LOCK_REC;
	lock->index = index;
	
	lock->un_member.rec_lock.space = space;
	lock->un_member.rec_lock.page_no = page_no;
	lock->un_member.rec_lock.n_bits = n_bytes * 8;

	/* Reset to zero the bitmap which resides immediately after the
	lock struct */
    /*将紧挨着锁结构体的位图重置为零*/
	lock_rec_bitmap_reset(lock);

	/* Set the bit corresponding to rec */ /*设置对应于rec的位*/
	lock_rec_set_nth_bit(lock, heap_no);

	HASH_INSERT(lock_t, hash, lock_sys->rec_hash,
					lock_rec_fold(space, page_no), lock); 
	if (type_mode & LOCK_WAIT) {

		lock_set_lock_and_trx_wait(lock, trx);
	}
	
	return(lock);
}

/*************************************************************************
Enqueues a waiting request for a lock which cannot be granted immediately.
Checks for deadlocks. */ /*对一个不能立即授予的锁的等待请求进行排队。检查死锁。*/
static
ulint
lock_rec_enqueue_waiting(
/*=====================*/
				/* out: DB_LOCK_WAIT, DB_DEADLOCK, or
				DB_QUE_THR_SUSPENDED */
	ulint		type_mode,/* in: lock mode this transaction is
				requesting: LOCK_S or LOCK_X, ORed with
				LOCK_GAP if a gap lock is requested */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t*	thr)	/* in: query thread */
{
	lock_t*	lock;
	trx_t*	trx;
	
	ut_ad(mutex_own(&kernel_mutex));

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */
    /*测试是否已经有其他原因挂起线程:如果查询线程应该停止，我们不会将锁请求排队*/
	if (que_thr_stop(thr)) {

		return(DB_QUE_THR_SUSPENDED);
	}
		
	trx = thr_get_trx(thr);

	/* Enqueue the lock request that will wait to be granted *//*将等待被授予的锁请求排队*/
	lock = lock_rec_create(type_mode | LOCK_WAIT, rec, index, trx);

	/* Check if a deadlock occurs: if yes, remove the lock request and
	return an error code */
	/*检查是否发生死锁:如果是，移除锁请求并返回错误代码*/
	if (lock_deadlock_occurs(lock, trx)) {

		lock_reset_lock_and_trx_wait(lock);
		lock_rec_reset_nth_bit(lock, rec_get_heap_no(rec));

		return(DB_DEADLOCK);
	}

	trx->que_state = TRX_QUE_LOCK_WAIT;

	ut_a(que_thr_stop(thr));

	if (lock_print_waits) {
		printf("Lock wait for trx %lu in index %s\n",
				ut_dulint_get_low(trx->id), index->name);
	}
	
	return(DB_LOCK_WAIT);	
}

/*************************************************************************
Adds a record lock request in the record queue. The request is normally
added as the last in the queue, but if there are no waiting lock requests
on the record, and the request to be added is not a waiting request, we
can reuse a suitable record lock object already existing on the same page,
just setting the appropriate bit in its bitmap. This is a low-level function
which does NOT check for deadlocks or lock compatibility! */
/*在记录队列中添加一个记录锁定请求。请求在队列中通常是作为最后添加,但如果没有等待锁请求记录,和请求添加并不是一个等待请求,
我们可以重用一个合适的记录锁对象已经存在在同一页面,设置适当的位的位图。这是一个低级函数，不检查死锁或锁兼容性!*/
static
lock_t*
lock_rec_add_to_queue(
/*==================*/
				/* out: lock where the bit was set, NULL if out
				of memory */
	ulint		type_mode,/* in: lock mode, wait, and gap flags; type
				is ignored and replaced by LOCK_REC */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: index of record */
	trx_t*		trx)	/* in: transaction */
{
	lock_t*	lock;
	lock_t*	similar_lock	= NULL;
	ulint	heap_no;
	page_t*	page;
	ibool	somebody_waits	= FALSE;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((type_mode & (LOCK_WAIT | LOCK_GAP))
	      || ((type_mode & LOCK_MODE_MASK) != LOCK_S)
	      || !lock_rec_other_has_expl_req(LOCK_X, 0, LOCK_WAIT, rec, trx));
	ut_ad((type_mode & (LOCK_WAIT | LOCK_GAP))
	      || ((type_mode & LOCK_MODE_MASK) != LOCK_X)
	      || !lock_rec_other_has_expl_req(LOCK_S, 0, LOCK_WAIT, rec, trx));
	      
	type_mode = type_mode | LOCK_REC;

	page = buf_frame_align(rec);

	/* If rec is the supremum record, then we can reset the gap bit, as
	all locks on the supremum are automatically of the gap type, and we
	try to avoid unnecessary memory consumption of a new record lock
	struct for a gap type lock */
    /*如果rec是上记录，那么我们可以重置间隙位，因为上记录上的所有锁都自动是gap类型的，并且我们尽量避免间隙类型锁的新记录锁结构的不必要内存消耗*/
	if (rec == page_get_supremum_rec(page)) {

		type_mode = type_mode & ~LOCK_GAP;
	}

	/* Look for a waiting lock request on the same record, or for a
	similar record lock on the same page */
    /*在同一记录上寻找一个等待的锁请求，或者在同一页上寻找一个类似的记录锁*/
	heap_no = rec_get_heap_no(rec);
	lock = lock_rec_get_first_on_page(rec);

	while (lock != NULL) {
		if (lock_get_wait(lock)
				&& (lock_rec_get_nth_bit(lock, heap_no))) {

			somebody_waits = TRUE;
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	similar_lock = lock_rec_find_similar_on_page(type_mode, rec, trx);

	if (similar_lock && !somebody_waits && !(type_mode & LOCK_WAIT)) {

		lock_rec_set_nth_bit(similar_lock, heap_no);

		return(similar_lock);
	}

	return(lock_rec_create(type_mode, rec, index, trx));
}

/*************************************************************************
This is a fast routine for locking a record in the most common cases:
there are no explicit locks on the page, or there is just one lock, owned
by this transaction, and of the right type_mode. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. */
UNIV_INLINE
ibool
lock_rec_lock_fast(
/*===============*/
				/* out: TRUE if locking succeeded */
	ibool		impl,	/* in: if TRUE, no lock is set if no wait
				is necessary: we assume that the caller will
				set an implicit lock */
	ulint		mode,	/* in: lock mode */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t* 	thr)	/* in: query thread */
{
	lock_t*	lock;
	ulint	heap_no;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((mode == LOCK_X) || (mode == LOCK_S));

	heap_no = rec_get_heap_no(rec);
	
	lock = lock_rec_get_first_on_page(rec);

	if (lock == NULL) {
		if (!impl) {
			lock_rec_create(mode, rec, index, thr_get_trx(thr));
		}
		
		return(TRUE);
	}
	
	if (lock_rec_get_next_on_page(lock)) {

		return(FALSE);
	}

	if (lock->trx != thr_get_trx(thr)
				|| lock->type_mode != (mode | LOCK_REC)
				|| lock_rec_get_n_bits(lock) <= heap_no) {
	    	return(FALSE);
	}

	if (!impl) {
		lock_rec_set_nth_bit(lock, heap_no);
	}

	return(TRUE);
}

/*************************************************************************
This is the general, and slower, routine for locking a record. This is a
low-level function which does NOT look at implicit locks! Checks lock
compatibility within explicit locks. */
static
ulint
lock_rec_lock_slow(
/*===============*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				code */
	ibool		impl,	/* in: if TRUE, no lock is set if no wait is
				necessary: we assume that the caller will set
				an implicit lock */
	ulint		mode,	/* in: lock mode */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t* 	thr)	/* in: query thread */
{
	ulint	confl_mode;
	trx_t*	trx;
	ulint	err;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((mode == LOCK_X) || (mode == LOCK_S));

	trx = thr_get_trx(thr);
	confl_mode = lock_get_confl_mode(mode);

	ut_ad((mode != LOCK_S) || lock_table_has(trx, index->table,
								LOCK_IS));
	ut_ad((mode != LOCK_X) || lock_table_has(trx, index->table,
								LOCK_IX));
	if (lock_rec_has_expl(mode, rec, trx)) {
		/* The trx already has a strong enough lock on rec: do
		nothing */

		err = DB_SUCCESS;
	} else if (lock_rec_other_has_expl_req(confl_mode, 0, LOCK_WAIT, rec,
								trx)) {
		/* If another transaction has a non-gap conflicting request in
		the queue, as this transaction does not have a lock strong
		enough already granted on the record, we have to wait. */
    				
		err = lock_rec_enqueue_waiting(mode, rec, index, thr);
	} else {
		if (!impl) {
			/* Set the requested lock on the record */

			lock_rec_add_to_queue(LOCK_REC | mode, rec, index,
									trx);
		}

		err = DB_SUCCESS;
	}

	return(err);
}

/*************************************************************************
Tries to lock the specified record in the mode requested. If not immediately
possible, enqueues a waiting lock request. This is a low-level function
which does NOT look at implicit locks! Checks lock compatibility within
explicit locks. */

ulint
lock_rec_lock(
/*==========*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				code */
	ibool		impl,	/* in: if TRUE, no lock is set if no wait is
				necessary: we assume that the caller will set
				an implicit lock */
	ulint		mode,	/* in: lock mode */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of record */
	que_thr_t* 	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad((mode != LOCK_S) || lock_table_has(thr_get_trx(thr),
						index->table, LOCK_IS));
	ut_ad((mode != LOCK_X) || lock_table_has(thr_get_trx(thr),
						index->table, LOCK_IX));

	if (lock_rec_lock_fast(impl, mode, rec, index, thr)) {

		/* We try a simplified and faster subroutine for the most
		common cases */

		err = DB_SUCCESS;
	} else {
		err = lock_rec_lock_slow(impl, mode, rec, index, thr);
	}

	return(err);
}

/*************************************************************************
Checks if a waiting record lock request still has to wait in a queue.
NOTE that we, for simplicity, ignore the gap bits in locks, and treat
gap type lock requests like non-gap lock requests. */
static
ibool
lock_rec_has_to_wait_in_queue(
/*==========================*/
				/* out: TRUE if still has to wait */
	lock_t*	wait_lock)	/* in: waiting record lock */
{
	lock_t*	lock;
	ulint	space;
	ulint	page_no;
	ulint	heap_no;

	ut_ad(mutex_own(&kernel_mutex));
 	ut_ad(lock_get_wait(wait_lock));
 	
	space = wait_lock->un_member.rec_lock.space;
	page_no = wait_lock->un_member.rec_lock.page_no;
	heap_no = lock_rec_find_set_bit(wait_lock);

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	while (lock != wait_lock) {

		if (lock_has_to_wait(wait_lock, lock)
				&& lock_rec_get_nth_bit(lock, heap_no)) {

		    	return(TRUE);
		}

		lock = lock_rec_get_next_on_page(lock);
	}

	return(FALSE);
}

/*****************************************************************
Grants a lock to a waiting lock request and releases the waiting
transaction. */

void
lock_grant(
/*=======*/
	lock_t*	lock)	/* in: waiting lock request */
{
	ut_ad(mutex_own(&kernel_mutex));

	lock_reset_lock_and_trx_wait(lock);

	if (lock_print_waits) {
		printf("Lock wait for trx %lu ends\n",
					ut_dulint_get_low(lock->trx->id));
	}
	
	trx_end_lock_wait(lock->trx);
}

/*****************************************************************
Cancels a waiting record lock request and releases the waiting transaction
that requested it. NOTE: does NOT check if waiting lock requests behind this
one can now be granted! */
static
void
lock_rec_cancel(
/*============*/
	lock_t*	lock)	/* in: waiting record lock request */
{
	ut_ad(mutex_own(&kernel_mutex));

	/* Reset the bit (there can be only one set bit) in the lock bitmap */
	lock_rec_reset_nth_bit(lock, lock_rec_find_set_bit(lock));

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_end_lock_wait(lock->trx);
}
	
/*****************************************************************
Removes a record lock request, waiting or granted, from the queue and
grants locks to other transactions in the queue if they now are entitled
to a lock. NOTE: all record locks contained in in_lock are removed. */

void
lock_rec_dequeue_from_page(
/*=======================*/
	lock_t*	in_lock)/* in: record lock object: all record locks which
			are contained in this lock object are removed;
			transactions waiting behind will get their lock
			requests granted, if they are now qualified to it */
{
	ulint	space;
	ulint	page_no;
	lock_t*	lock;
	trx_t*	trx;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type(in_lock) == LOCK_REC);

	trx = in_lock->trx;
	
	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
				lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, in_lock);

	/* Check if waiting locks in the queue can now be granted: grant
	locks if there are no conflicting locks ahead. */

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	while (lock != NULL) {		
		if (lock_get_wait(lock)
				&& !lock_rec_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			lock_grant(lock);
		}

		lock = lock_rec_get_next_on_page(lock);
	}
}	

/*****************************************************************
Removes a record lock request, waiting or granted, from the queue. */
static
void
lock_rec_discard(
/*=============*/
	lock_t*	in_lock)/* in: record lock object: all record locks which
			are contained in this lock object are removed */
{
	ulint	space;
	ulint	page_no;
	trx_t*	trx;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type(in_lock) == LOCK_REC);

	trx = in_lock->trx;
	
	space = in_lock->un_member.rec_lock.space;
	page_no = in_lock->un_member.rec_lock.page_no;

	HASH_DELETE(lock_t, hash, lock_sys->rec_hash,
				lock_rec_fold(space, page_no), in_lock);

	UT_LIST_REMOVE(trx_locks, trx->trx_locks, in_lock);
}

/*****************************************************************
Removes record lock objects set on an index page which is discarded. This
function does not move locks, or check for waiting locks, therefore the
lock bitmaps must already be reset when this function is called. */
static
void
lock_rec_free_all_from_discard_page(
/*================================*/
	page_t*	page)	/* in: page to be discarded */
{
	ulint	space;
	ulint	page_no;
	lock_t*	lock;
	lock_t*	next_lock;
	
	ut_ad(mutex_own(&kernel_mutex));
	
	space = buf_frame_get_space_id(page);
	page_no = buf_frame_get_page_no(page);

	lock = lock_rec_get_first_on_page_addr(space, page_no);

	while (lock != NULL) {
		ut_ad(lock_rec_find_set_bit(lock) == ULINT_UNDEFINED);
		ut_ad(!lock_get_wait(lock));

		next_lock = lock_rec_get_next_on_page(lock);
		
		lock_rec_discard(lock);
		
		lock = next_lock;
	}
}	

/*============= RECORD LOCK MOVING AND INHERITING ===================*/

/*****************************************************************
Resets the lock bits for a single record. Releases transactions waiting for
lock requests here. */

void
lock_rec_reset_and_release_wait(
/*============================*/
	rec_t*	rec)	/* in: record whose locks bits should be reset */
{
	lock_t*	lock;
	ulint	heap_no;
	
	ut_ad(mutex_own(&kernel_mutex));

	heap_no = rec_get_heap_no(rec);
	
	lock = lock_rec_get_first(rec);

	while (lock != NULL) {
		if (lock_get_wait(lock)) {
			lock_rec_cancel(lock);
		} else {
			lock_rec_reset_nth_bit(lock, heap_no);
		}

		lock = lock_rec_get_next(rec, lock);
	}
}	

/*****************************************************************
Makes a record to inherit the locks of another record as gap type locks, but
does not reset the lock bits of the other record. Also waiting lock requests
on rec are inherited as GRANTED gap locks. */

void
lock_rec_inherit_to_gap(
/*====================*/
	rec_t*	heir,	/* in: record which inherits */
	rec_t*	rec)	/* in: record from which inherited; does NOT reset
			the locks on this record */
{
	lock_t*	lock;
	
	ut_ad(mutex_own(&kernel_mutex));
	
	lock = lock_rec_get_first(rec);

	while (lock != NULL) {
		lock_rec_add_to_queue((lock->type_mode | LOCK_GAP) & ~LOCK_WAIT,
	 			     		heir, lock->index, lock->trx);
		lock = lock_rec_get_next(rec, lock);
	}
}	

/*****************************************************************
Moves the locks of a record to another record and resets the lock bits of
the donating record. */
static
void
lock_rec_move(
/*==========*/
	rec_t*	receiver,	/* in: record which gets locks; this record
				must have no lock requests on it! */
	rec_t*	donator)	/* in: record which gives locks */
{
	lock_t*	lock;
	ulint	heap_no;
	ulint	type_mode;
	
	ut_ad(mutex_own(&kernel_mutex));

	heap_no = rec_get_heap_no(donator);
	
	lock = lock_rec_get_first(donator);

	ut_ad(lock_rec_get_first(receiver) == NULL);

	while (lock != NULL) {
		type_mode = lock->type_mode;
	
		lock_rec_reset_nth_bit(lock, heap_no);

		if (lock_get_wait(lock)) {
			lock_reset_lock_and_trx_wait(lock);
		}	

		/* Note that we FIRST reset the bit, and then set the lock:
		the function works also if donator == receiver */

		lock_rec_add_to_queue(type_mode, receiver, lock->index,
								lock->trx);
		lock = lock_rec_get_next(donator, lock);
	}

	ut_ad(lock_rec_get_first(donator) == NULL);
}	

/*****************************************************************
Updates the lock table when we have reorganized a page. NOTE: we copy
also the locks set on the infimum of the page; the infimum may carry
locks if an update of a record is occurring on the page, and its locks
were temporarily stored on the infimum. */

void
lock_move_reorganize_page(
/*======================*/
	page_t*	page,		/* in: old index page, now reorganized */
	page_t*	old_page)	/* in: copy of the old, not reorganized page */
{
	lock_t*		lock;
	lock_t*		old_lock;
	page_cur_t	cur1;
	page_cur_t	cur2;
	ulint		old_heap_no;
	UT_LIST_BASE_NODE_T(lock_t)	old_locks;
	mem_heap_t*	heap		= NULL;
	rec_t*		sup;

	lock_mutex_enter_kernel();

	lock = lock_rec_get_first_on_page(page);

	if (lock == NULL) {
		lock_mutex_exit_kernel();

		return;
	}

	heap = mem_heap_create(256);
	
	/* Copy first all the locks on the page to heap and reset the
	bitmaps in the original locks; chain the copies of the locks
	using the trx_locks field in them. */

	UT_LIST_INIT(old_locks);
	
	while (lock != NULL) {

		/* Make a copy of the lock */
		old_lock = lock_rec_copy(lock, heap);

		UT_LIST_ADD_LAST(trx_locks, old_locks, old_lock);

		/* Reset bitmap of lock */
		lock_rec_bitmap_reset(lock);

		if (lock_get_wait(lock)) {
			lock_reset_lock_and_trx_wait(lock);
		}		

		lock = lock_rec_get_next_on_page(lock);
	}

	sup = page_get_supremum_rec(page);
	
	lock = UT_LIST_GET_FIRST(old_locks);

	while (lock) {
		/* NOTE: we copy also the locks set on the infimum and
		supremum of the page; the infimum may carry locks if an
		update of a record is occurring on the page, and its locks
		were temporarily stored on the infimum */
		
		page_cur_set_before_first(page, &cur1);
		page_cur_set_before_first(old_page, &cur2);

		/* Set locks according to old locks */
		for (;;) {
			ut_ad(0 == ut_memcmp(page_cur_get_rec(&cur1),
						page_cur_get_rec(&cur2),
						rec_get_data_size(
						   page_cur_get_rec(&cur2))));
		
			old_heap_no = rec_get_heap_no(page_cur_get_rec(&cur2));

			if (lock_rec_get_nth_bit(lock, old_heap_no)) {

				/* NOTE that the old lock bitmap could be too
				small for the new heap number! */

				lock_rec_add_to_queue(lock->type_mode,
						page_cur_get_rec(&cur1),
						lock->index, lock->trx);

				/* if ((page_cur_get_rec(&cur1) == sup)
						&& lock_get_wait(lock)) {
					printf(
				"---\n--\n!!!Lock reorg: supr type %lu\n",
					lock->type_mode);
				} */
			}

			if (page_cur_get_rec(&cur1) == sup) {

				break;
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

		/* Remember that we chained old locks on the trx_locks field: */

		lock = UT_LIST_GET_NEXT(trx_locks, lock);
	}

	lock_mutex_exit_kernel();

	mem_heap_free(heap);

/* 	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page))); */
}	

/*****************************************************************
Moves the explicit locks on user records to another page if a record
list end is moved to another page. */

void
lock_move_rec_list_end(
/*===================*/
	page_t*	new_page,	/* in: index page to move to */
	page_t*	page,		/* in: index page */
	rec_t*	rec)		/* in: record on page: this is the
				first record moved */
{
	lock_t*		lock;
	page_cur_t	cur1;
	page_cur_t	cur2;
	ulint		heap_no;
	rec_t*		sup;
	ulint		type_mode;
	
	lock_mutex_enter_kernel();

	/* Note: when we move locks from record to record, waiting locks
	and possible granted gap type locks behind them are enqueued in
	the original order, because new elements are inserted to a hash
	table to the end of the hash chain, and lock_rec_add_to_queue
	does not reuse locks if there are waiters in the queue. */

	sup = page_get_supremum_rec(page);
	
	lock = lock_rec_get_first_on_page(page);

	while (lock != NULL) {
		
		page_cur_position(rec, &cur1);

		if (page_cur_is_before_first(&cur1)) {
			page_cur_move_to_next(&cur1);
		}

		page_cur_set_before_first(new_page, &cur2);
		page_cur_move_to_next(&cur2);
	
		/* Copy lock requests on user records to new page and
		reset the lock bits on the old */

		while (page_cur_get_rec(&cur1) != sup) {

			ut_ad(0 == ut_memcmp(page_cur_get_rec(&cur1),
						page_cur_get_rec(&cur2),
						rec_get_data_size(
						   page_cur_get_rec(&cur2))));
		
			heap_no = rec_get_heap_no(page_cur_get_rec(&cur1));

			if (lock_rec_get_nth_bit(lock, heap_no)) {
				type_mode = lock->type_mode;

				lock_rec_reset_nth_bit(lock, heap_no);

				if (lock_get_wait(lock)) {
					lock_reset_lock_and_trx_wait(lock);
				}	

				lock_rec_add_to_queue(type_mode,
						page_cur_get_rec(&cur2),
						lock->index, lock->trx);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

		lock = lock_rec_get_next_on_page(lock);
	}
	
	lock_mutex_exit_kernel();

/*	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page)));
	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(new_page),
					buf_frame_get_page_no(new_page))); */
}	

/*****************************************************************
Moves the explicit locks on user records to another page if a record
list start is moved to another page. */

void
lock_move_rec_list_start(
/*=====================*/
	page_t*	new_page,	/* in: index page to move to */
	page_t*	page,		/* in: index page */
	rec_t*	rec,		/* in: record on page: this is the
				first record NOT copied */
	rec_t*	old_end)	/* in: old previous-to-last record on
				new_page before the records were copied */
{
	lock_t*		lock;
	page_cur_t	cur1;
	page_cur_t	cur2;
	ulint		heap_no;
	ulint		type_mode;

	ut_ad(new_page);

	lock_mutex_enter_kernel();

	lock = lock_rec_get_first_on_page(page);

	while (lock != NULL) {
		
		page_cur_set_before_first(page, &cur1);
		page_cur_move_to_next(&cur1);

		page_cur_position(old_end, &cur2);
		page_cur_move_to_next(&cur2);

		/* Copy lock requests on user records to new page and
		reset the lock bits on the old */

		while (page_cur_get_rec(&cur1) != rec) {

			ut_ad(0 == ut_memcmp(page_cur_get_rec(&cur1),
						page_cur_get_rec(&cur2),
						rec_get_data_size(
						   page_cur_get_rec(&cur2))));
		
			heap_no = rec_get_heap_no(page_cur_get_rec(&cur1));

			if (lock_rec_get_nth_bit(lock, heap_no)) {
				type_mode = lock->type_mode;

				lock_rec_reset_nth_bit(lock, heap_no);

				if (lock_get_wait(lock)) {
					lock_reset_lock_and_trx_wait(lock);
				}			

				lock_rec_add_to_queue(type_mode,
						page_cur_get_rec(&cur2),
						lock->index, lock->trx);
			}

			page_cur_move_to_next(&cur1);
			page_cur_move_to_next(&cur2);
		}

		lock = lock_rec_get_next_on_page(lock);
	}
	
	lock_mutex_exit_kernel();

/*	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(page),
					buf_frame_get_page_no(page)));
	ut_ad(lock_rec_validate_page(buf_frame_get_space_id(new_page),
					buf_frame_get_page_no(new_page))); */
}	

/*****************************************************************
Updates the lock table when a page is split to the right. */

void
lock_update_split_right(
/*====================*/
	page_t*	right_page,	/* in: right page */
	page_t*	left_page)	/* in: left page */
{
	lock_mutex_enter_kernel();
	
	/* Move the locks on the supremum of the left page to the supremum
	of the right page */

	lock_rec_move(page_get_supremum_rec(right_page),
					page_get_supremum_rec(left_page));
	
	/* Inherit the locks to the supremum of left page from the successor
	of the infimum on right page */

	lock_rec_inherit_to_gap(page_get_supremum_rec(left_page),
			page_rec_get_next(page_get_infimum_rec(right_page)));

	lock_mutex_exit_kernel();
}	

/*****************************************************************
Updates the lock table when a page is merged to the right. */

void
lock_update_merge_right(
/*====================*/
	rec_t*	orig_succ,	/* in: original successor of infimum
				on the right page before merge */
	page_t*	left_page)	/* in: merged index page which will be
				discarded */
{
	lock_mutex_enter_kernel();
	
	/* Inherit the locks from the supremum of the left page to the
	original successor of infimum on the right page, to which the left
	page was merged */

	lock_rec_inherit_to_gap(orig_succ, page_get_supremum_rec(left_page));

	/* Reset the locks on the supremum of the left page, releasing
	waiting transactions */

	lock_rec_reset_and_release_wait(page_get_supremum_rec(left_page));
	
	lock_rec_free_all_from_discard_page(left_page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when the root page is copied to another in
btr_root_raise_and_insert. Note that we leave lock structs on the
root page, even though they do not make sense on other than leaf
pages: the reason is that in a pessimistic update the infimum record
of the root page will act as a dummy carrier of the locks of the record
to be updated. */

void
lock_update_root_raise(
/*===================*/
	page_t*	new_page,	/* in: index page to which copied */
	page_t*	root)		/* in: root page */
{
	lock_mutex_enter_kernel();
	
	/* Move the locks on the supremum of the root to the supremum
	of new_page */

	lock_rec_move(page_get_supremum_rec(new_page),
						page_get_supremum_rec(root));
	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a page is copied to another and the original page
is removed from the chain of leaf pages, except if page is the root! */

void
lock_update_copy_and_discard(
/*=========================*/
	page_t*	new_page,	/* in: index page to which copied */
	page_t*	page)		/* in: index page; NOT the root! */
{
	lock_mutex_enter_kernel();
	
	/* Move the locks on the supremum of the old page to the supremum
	of new_page */

	lock_rec_move(page_get_supremum_rec(new_page),
						page_get_supremum_rec(page));
	lock_rec_free_all_from_discard_page(page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a page is split to the left. */

void
lock_update_split_left(
/*===================*/
	page_t*	right_page,	/* in: right page */
	page_t*	left_page)	/* in: left page */
{
	lock_mutex_enter_kernel();
	
	/* Inherit the locks to the supremum of the left page from the
	successor of the infimum on the right page */

	lock_rec_inherit_to_gap(page_get_supremum_rec(left_page),
			page_rec_get_next(page_get_infimum_rec(right_page)));

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a page is merged to the left. */

void
lock_update_merge_left(
/*===================*/
	page_t*	left_page,	/* in: left page to which merged */
	rec_t*	orig_pred,	/* in: original predecessor of supremum
				on the left page before merge */
	page_t*	right_page)	/* in: merged index page which will be
				discarded */
{
	lock_mutex_enter_kernel();
	
	if (page_rec_get_next(orig_pred) != page_get_supremum_rec(left_page)) {

		/* Inherit the locks on the supremum of the left page to the
		first record which was moved from the right page */

		lock_rec_inherit_to_gap(page_rec_get_next(orig_pred),
					page_get_supremum_rec(left_page));

		/* Reset the locks on the supremum of the left page,
		releasing waiting transactions */

		lock_rec_reset_and_release_wait(page_get_supremum_rec(
								left_page));
	}

	/* Move the locks from the supremum of right page to the supremum
	of the left page */
	
	lock_rec_move(page_get_supremum_rec(left_page),
					 page_get_supremum_rec(right_page));

	lock_rec_free_all_from_discard_page(right_page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Resets the original locks on heir and replaces them with gap type locks
inherited from rec. */

void
lock_rec_reset_and_inherit_gap_locks(
/*=================================*/
	rec_t*	heir,	/* in: heir record */
	rec_t*	rec)	/* in: record */
{
	mutex_enter(&kernel_mutex);	      				

	lock_rec_reset_and_release_wait(heir);
	
	lock_rec_inherit_to_gap(heir, rec);

	mutex_exit(&kernel_mutex);	      				
}

/*****************************************************************
Updates the lock table when a page is discarded. */

void
lock_update_discard(
/*================*/
	rec_t*	heir,	/* in: record which will inherit the locks */
	page_t*	page)	/* in: index page which will be discarded */
{
	rec_t*	rec;

	lock_mutex_enter_kernel();
	
	if (NULL == lock_rec_get_first_on_page(page)) {
		/* No locks exist on page, nothing to do */

		lock_mutex_exit_kernel();

		return;
	}
	
	/* Inherit all the locks on the page to the record and reset all
	the locks on the page */

	rec = page_get_infimum_rec(page);

	for (;;) {
		lock_rec_inherit_to_gap(heir, rec);

		/* Reset the locks on rec, releasing waiting transactions */

		lock_rec_reset_and_release_wait(rec);

		if (rec == page_get_supremum_rec(page)) {

			break;
		}
		
		rec = page_rec_get_next(rec);
	}

	lock_rec_free_all_from_discard_page(page);

	lock_mutex_exit_kernel();
}

/*****************************************************************
Updates the lock table when a new user record is inserted. */

void
lock_update_insert(
/*===============*/
	rec_t*	rec)	/* in: the inserted record */
{
	lock_mutex_enter_kernel();

	/* Inherit the locks for rec, in gap mode, from the next record */

	lock_rec_inherit_to_gap(rec, page_rec_get_next(rec));

	lock_mutex_exit_kernel();
}	

/*****************************************************************
Updates the lock table when a record is removed. */

void
lock_update_delete(
/*===============*/
	rec_t*	rec)	/* in: the record to be removed */
{
	lock_mutex_enter_kernel();

	/* Let the next record inherit the locks from rec, in gap mode */

	lock_rec_inherit_to_gap(page_rec_get_next(rec), rec);

	/* Reset the lock bits on rec and release waiting transactions */

	lock_rec_reset_and_release_wait(rec);	

	lock_mutex_exit_kernel();
}
 
/*************************************************************************
Stores on the page infimum record the explicit locks of another record.
This function is used to store the lock state of a record when it is
updated and the size of the record changes in the update. The record
is moved in such an update, perhaps to another page. The infimum record
acts as a dummy carrier record, taking care of lock releases while the
actual record is being moved. */

void
lock_rec_store_on_page_infimum(
/*===========================*/
	rec_t*	rec)	/* in: record whose lock state is stored
			on the infimum record of the same page; lock
			bits are reset on the record */
{
	page_t*	page;

	page = buf_frame_align(rec);

	lock_mutex_enter_kernel();
	
	lock_rec_move(page_get_infimum_rec(page), rec);

	lock_mutex_exit_kernel();	
}

/*************************************************************************
Restores the state of explicit lock requests on a single record, where the
state was stored on the infimum of the page. */

void
lock_rec_restore_from_page_infimum(
/*===============================*/
	rec_t*	rec,	/* in: record whose lock state is restored */
	page_t*	page)	/* in: page (rec is not necessarily on this page)
			whose infimum stored the lock state; lock bits are
			reset on the infimum */ 
{
	lock_mutex_enter_kernel();
	
	lock_rec_move(rec, page_get_infimum_rec(page));
	
	lock_mutex_exit_kernel();
}

/*=========== DEADLOCK CHECKING ======================================*/

/************************************************************************
Checks if a lock request results in a deadlock. */ /*检查锁请求是否导致死锁。*/
static
ibool
lock_deadlock_occurs(
/*=================*/
			/* out: TRUE if a deadlock was detected */ /*out:如果检测到死锁，则为TRUE*/
	lock_t*	lock,	/* in: lock the transaction is requesting */
	trx_t*	trx)	/* in: transaction */
{
	dict_table_t*	table;
	dict_index_t*	index;
	trx_t*		mark_trx;
	ibool		ret;
	ulint		cost	= 0;

	ut_ad(trx && lock);
	ut_ad(mutex_own(&kernel_mutex));

	/* We check that adding this trx to the waits-for graph
	does not produce a cycle. First mark all active transactions
	with 0: */
    /*我们检查将这个trx添加到waiting -for图中不会产生一个循环。首先将所有活动事务标记为0:*/
	mark_trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (mark_trx) {
		mark_trx->deadlock_mark = 0;
		mark_trx = UT_LIST_GET_NEXT(trx_list, mark_trx);
	}

	ret = lock_deadlock_recursive(trx, trx, lock, &cost);

	if (ret) {
		if (lock_get_type(lock) == LOCK_TABLE) {
			table = lock->un_member.tab_lock.table;
			index = NULL;
		} else {
			index = lock->index;
			table = index->table;
		}
		/*
		sess_raise_error_low(trx, DB_DEADLOCK, lock->type_mode, table,
						index, NULL, NULL, NULL);
		*/
	}
	
	return(ret);
}

/************************************************************************
Looks recursively for a deadlock. */ /*递归查找死锁。*/
static
ibool
lock_deadlock_recursive(
/*====================*/
				/* out: TRUE if a deadlock was detected
				or the calculation took too long */ /*out:如果检测到死锁或计算时间过长，则为TRUE*/
	trx_t*	start,		/* in: recursion starting point */ /*递归起点*/
	trx_t*	trx,		/* in: a transaction waiting for a lock */ /*等待锁的事务*/
	lock_t*	wait_lock,	/* in: the lock trx is waiting to be granted */ /*锁定TRX正在等待被授予*/
	ulint*	cost)		/* in/out: number of calculation steps thus
				far: if this exceeds LOCK_MAX_N_STEPS_... 
				we return TRUE */ /* 到目前为止的计算步骤数:如果这个超过LOCK_MAX_N_STEPS_…我们返回真 */
{
	lock_t*	lock;
	ulint	bit_no;
	trx_t*	lock_trx;
	
	ut_a(trx && start && wait_lock);
	ut_ad(mutex_own(&kernel_mutex));
	
	if (trx->deadlock_mark == 1) {
		/* We have already exhaustively searched the subtree starting
		from this trx */
        /*我们已经从这个trx开始详尽地搜索了子树*/
		return(FALSE);
	}

	*cost = *cost + 1;

	if (*cost > LOCK_MAX_N_STEPS_IN_DEADLOCK_CHECK) {

		return(TRUE);
	}

	lock = wait_lock;

	if (lock_get_type(wait_lock) == LOCK_REC) {

		bit_no = lock_rec_find_set_bit(wait_lock);

		ut_a(bit_no != ULINT_UNDEFINED);
	}

	/* Look at the locks ahead of wait_lock in the lock queue */
    /* 查看锁队列中wait_lock前面的锁*/
	for (;;) {
		if (lock_get_type(lock) == LOCK_TABLE) {

			lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
		} else {
			ut_ad(lock_get_type(lock) == LOCK_REC);

			lock = lock_rec_get_prev(lock, bit_no);
		}

		if (lock == NULL) {
			/* We can mark this subtree as searched */
			trx->deadlock_mark = 1;

			return(FALSE);
		}

		if (lock_has_to_wait(wait_lock, lock)) {

			lock_trx = lock->trx;

			if (lock_trx == start) {
				if (lock_print_waits) {
					printf("Deadlock detected\n");
				}

				return(TRUE);
			}
	
			if (lock_trx->que_state == TRX_QUE_LOCK_WAIT) {

				/* Another trx ahead has requested lock	in an
				incompatible mode, and is itself waiting for
				a lock */
                /*前面的另一个trx以不兼容的模式请求锁定，并且本身正在等待锁定*/
				if (lock_deadlock_recursive(start, lock_trx,
						lock_trx->wait_lock, cost)) {

					return(TRUE);
				}
			}
		}
	}/* end of the 'for (;;)'-loop */
}

/*========================= TABLE LOCKS ==============================*/

/*************************************************************************
Creates a table lock object and adds it as the last in the lock queue
of the table. Does NOT check for deadlocks or lock compatibility. */
UNIV_INLINE
lock_t*
lock_table_create(
/*==============*/
				/* out, own: new lock object, or NULL if
				out of memory */
	dict_table_t*	table,	/* in: database table in dictionary cache */
	ulint		type_mode,/* in: lock mode possibly ORed with
				LOCK_WAIT */
	trx_t*		trx)	/* in: trx */
{
	lock_t*	lock;

	ut_ad(table && trx);
	ut_ad(mutex_own(&kernel_mutex));

	if (type_mode == LOCK_AUTO_INC) {
		/* Only one trx can have the lock on the table
		at a time: we may use the memory preallocated
		to the table object */

		lock = table->auto_inc_lock;

		ut_a(trx->auto_inc_lock == NULL);
		trx->auto_inc_lock = lock;
	} else {
		lock = mem_heap_alloc(trx->lock_heap, sizeof(lock_t));
	}

	if (lock == NULL) {

		return(NULL);
	}

	UT_LIST_ADD_LAST(trx_locks, trx->trx_locks, lock);

	lock->type_mode = type_mode | LOCK_TABLE;
	lock->trx = trx;

	lock->un_member.tab_lock.table = table;

	UT_LIST_ADD_LAST(un_member.tab_lock.locks, table->locks, lock);

	if (type_mode & LOCK_WAIT) {

		lock_set_lock_and_trx_wait(lock, trx);
	}

	return(lock);
}

/*****************************************************************
Removes a table lock request from the queue and the trx list of locks;
this is a low-level function which does NOT check if waiting requests
can now be granted. */
UNIV_INLINE
void
lock_table_remove_low(
/*==================*/
	lock_t*	lock)	/* in: table lock */
{
	dict_table_t*	table;
	trx_t*		trx;

	ut_ad(mutex_own(&kernel_mutex));

	table = lock->un_member.tab_lock.table;
	trx = lock->trx;

	if (lock == trx->auto_inc_lock) {
		trx->auto_inc_lock = NULL;
	}
	
	UT_LIST_REMOVE(trx_locks, trx->trx_locks, lock);
	UT_LIST_REMOVE(un_member.tab_lock.locks, table->locks, lock);
}	

/*************************************************************************
Enqueues a waiting request for a table lock which cannot be granted
immediately. Checks for deadlocks. */

ulint
lock_table_enqueue_waiting(
/*=======================*/
				/* out: DB_LOCK_WAIT, DB_DEADLOCK, or
				DB_QUE_THR_SUSPENDED */
	ulint		mode,	/* in: lock mode this transaction is
				requesting */
	dict_table_t*	table,	/* in: table */
	que_thr_t*	thr)	/* in: query thread */
{
	lock_t*	lock;
	trx_t*	trx;
	
	ut_ad(mutex_own(&kernel_mutex));

	/* Test if there already is some other reason to suspend thread:
	we do not enqueue a lock request if the query thread should be
	stopped anyway */

	if (que_thr_stop(thr)) {

		return(DB_QUE_THR_SUSPENDED);
	}

	trx = thr_get_trx(thr);
	
	/* Enqueue the lock request that will wait to be granted */

	lock = lock_table_create(table, mode | LOCK_WAIT, trx);

	/* Check if a deadlock occurs: if yes, remove the lock request and
	return an error code */

	if (lock_deadlock_occurs(lock, trx)) {

		lock_reset_lock_and_trx_wait(lock);
		lock_table_remove_low(lock);

		return(DB_DEADLOCK);
	}

	trx->que_state = TRX_QUE_LOCK_WAIT;

	ut_a(que_thr_stop(thr));

	return(DB_LOCK_WAIT);
}

/*************************************************************************
Checks if other transactions have an incompatible mode lock request in
the lock queue. */
UNIV_INLINE
ibool
lock_table_other_has_incompatible(
/*==============================*/
	trx_t*		trx,	/* in: transaction, or NULL if all
				transactions should be included */
	ulint		wait,	/* in: LOCK_WAIT if also waiting locks are
				taken into account, or 0 if not */
	dict_table_t*	table,	/* in: table */
	ulint		mode)	/* in: lock mode */
{
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_LAST(table->locks);

	while (lock != NULL) {

		if ((lock->trx != trx) 
		    && (!lock_mode_compatible(lock_get_mode(lock), mode))
		    && (wait || !(lock_get_wait(lock)))) {

		    	return(TRUE);
		}

		lock = UT_LIST_GET_PREV(un_member.tab_lock.locks, lock);
	}

	return(FALSE);
}

/*************************************************************************
Locks the specified database table in the mode given. If the lock cannot
be granted immediately, the query thread is put to wait. */

ulint
lock_table(
/*=======*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	dict_table_t*	table,	/* in: database table in dictionary cache */
	ulint		mode,	/* in: lock mode */
	que_thr_t*	thr)	/* in: query thread */
{
	trx_t*	trx;
	ulint	err;
	
	ut_ad(table && thr);

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	trx = thr_get_trx(thr);

	lock_mutex_enter_kernel();

	/* Look for stronger locks the same trx already has on the table */

	if (lock_table_has(trx, table, mode)) {

		lock_mutex_exit_kernel();

		return(DB_SUCCESS);
	}

	/* We have to check if the new lock is compatible with any locks
	other transactions have in the table lock queue. */

	if (lock_table_other_has_incompatible(trx, LOCK_WAIT, table, mode)) {
	
		/* Another trx has a request on the table in an incompatible
		mode: this trx must wait */

		err = lock_table_enqueue_waiting(mode, table, thr);
			
		lock_mutex_exit_kernel();

		return(err);
	}

	lock_table_create(table, mode, trx);

	lock_mutex_exit_kernel();

	return(DB_SUCCESS);
}

/*************************************************************************
Checks if there are any locks set on the table. */

ibool
lock_is_on_table(
/*=============*/
				/* out: TRUE if there are lock(s) */
	dict_table_t*	table)	/* in: database table in dictionary cache */
{
	ibool	ret;

	ut_ad(table);

	lock_mutex_enter_kernel();

	if (UT_LIST_GET_LAST(table->locks)) {
		ret = TRUE;
	} else {
		ret = FALSE;
	}

	lock_mutex_exit_kernel();

	return(ret);
}

/*************************************************************************
Checks if a waiting table lock request still has to wait in a queue. */
static
ibool
lock_table_has_to_wait_in_queue(
/*============================*/
				/* out: TRUE if still has to wait */
	lock_t*	wait_lock)	/* in: waiting table lock */
{
	dict_table_t*	table;
	lock_t*		lock;

 	ut_ad(lock_get_wait(wait_lock));
 	
	table = wait_lock->un_member.tab_lock.table;

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock != wait_lock) {

		if (lock_has_to_wait(wait_lock, lock)) {

		    	return(TRUE);
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
	}

	return(FALSE);
}

/*****************************************************************
Removes a table lock request, waiting or granted, from the queue and grants
locks to other transactions in the queue, if they now are entitled to a
lock. */

void
lock_table_dequeue(
/*===============*/
	lock_t*	in_lock)/* in: table lock object; transactions waiting
			behind will get their lock requests granted, if
			they are now qualified to it */
{
	lock_t*	lock;
	
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(lock_get_type(in_lock) == LOCK_TABLE);

	lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, in_lock);

	lock_table_remove_low(in_lock);
	
	/* Check if waiting locks in the queue can now be granted: grant
	locks if there are no conflicting locks ahead. */

	while (lock != NULL) {

		if (lock_get_wait(lock)
				&& !lock_table_has_to_wait_in_queue(lock)) {

			/* Grant the lock */
			lock_grant(lock);
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
	}
}	

/*=========================== LOCK RELEASE ==============================*/

/*************************************************************************
Releases an auto-inc lock a transaction possibly has on a table.
Releases possible other transactions waiting for this lock. */

void
lock_table_unlock_auto_inc(
/*=======================*/
	trx_t*	trx)	/* in: transaction */
{
	if (trx->auto_inc_lock) {
		mutex_enter(&kernel_mutex);

		lock_table_dequeue(trx->auto_inc_lock);

		mutex_exit(&kernel_mutex);
	}
}

/*************************************************************************
Releases transaction locks, and releases possible other transactions waiting
because of these locks. */

void
lock_release_off_kernel(
/*====================*/
	trx_t*	trx)	/* in: transaction */
{
	ulint	count;
	lock_t*	lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_LAST(trx->trx_locks);
	
	count = 0;

	while (lock != NULL) {

		count++;

		if (lock_get_type(lock) == LOCK_REC) {
			
			lock_rec_dequeue_from_page(lock);
		} else {
			ut_ad(lock_get_type(lock) == LOCK_TABLE);

			lock_table_dequeue(lock);
		}

		if (count == LOCK_RELEASE_KERNEL_INTERVAL) {
			/* Release the kernel mutex for a while, so that we
			do not monopolize it */

			lock_mutex_exit_kernel();

			lock_mutex_enter_kernel();

			count = 0;
		}	

		lock = UT_LIST_GET_LAST(trx->trx_locks);
	}

	mem_heap_empty(trx->lock_heap);

	ut_a(trx->auto_inc_lock == NULL);
}

/*************************************************************************
Cancels a waiting lock request and releases possible other transactions
waiting behind it. */

void
lock_cancel_waiting_and_release(
/*============================*/
	lock_t*	lock)	/* in: waiting lock request */
{
	ut_ad(mutex_own(&kernel_mutex));

	if (lock_get_type(lock) == LOCK_REC) {
			
		lock_rec_dequeue_from_page(lock);
	} else {
		ut_ad(lock_get_type(lock) == LOCK_TABLE);

		lock_table_dequeue(lock);
	}

	/* Reset the wait flag and the back pointer to lock in trx */

	lock_reset_lock_and_trx_wait(lock);

	/* The following function releases the trx from lock wait */

	trx_end_lock_wait(lock->trx);
}

/*************************************************************************
Resets all record and table locks of a transaction on a table to be dropped.
No lock is allowed to be a wait lock. */
static
void
lock_reset_all_on_table_for_trx(
/*============================*/
	dict_table_t*	table,	/* in: table to be dropped */
	trx_t*		trx)	/* in: a transaction */
{
	lock_t*	lock;
	lock_t*	prev_lock;

	ut_ad(mutex_own(&kernel_mutex));

	lock = UT_LIST_GET_LAST(trx->trx_locks);
	
	while (lock != NULL) {
		prev_lock = UT_LIST_GET_PREV(trx_locks, lock);
		
		if (lock_get_type(lock) == LOCK_REC
				&& lock->index->table == table) {
			ut_a(!lock_get_wait(lock));
			
			lock_rec_discard(lock);
		} else if (lock_get_type(lock) == LOCK_TABLE
				&& lock->un_member.tab_lock.table == table) {

			ut_a(!lock_get_wait(lock));
			
			lock_table_remove_low(lock);
		}

		lock = prev_lock;
	}
}

/*************************************************************************
Resets all locks, both table and record locks, on a table to be dropped.
No lock is allowed to be a wait lock. */

void
lock_reset_all_on_table(
/*====================*/
	dict_table_t*	table)	/* in: table to be dropped */
{
	lock_t*	lock;

	mutex_enter(&kernel_mutex);

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock) {	
		ut_a(!lock_get_wait(lock));

		lock_reset_all_on_table_for_trx(table, lock->trx);

		lock = UT_LIST_GET_FIRST(table->locks);
	}

	mutex_exit(&kernel_mutex);
}

/*===================== VALIDATION AND DEBUGGING  ====================*/

/*************************************************************************
Prints info of a table lock. */

void
lock_table_print(
/*=============*/
	lock_t*	lock)	/* in: table type lock */
{
	ut_ad(mutex_own(&kernel_mutex));
	ut_a(lock_get_type(lock) == LOCK_TABLE);

	printf("TABLE LOCK table %s trx id %lu %lu",
		lock->un_member.tab_lock.table->name,
		(lock->trx)->id.high, (lock->trx)->id.low);

	if (lock_get_mode(lock) == LOCK_S) {
		printf(" lock mode S");
	} else if (lock_get_mode(lock) == LOCK_X) {
		printf(" lock_mode X");
	} else if (lock_get_mode(lock) == LOCK_IS) {
		printf(" lock_mode IS");
	} else if (lock_get_mode(lock) == LOCK_IX) {
		printf(" lock_mode IX");
	} else if (lock_get_mode(lock) == LOCK_AUTO_INC) {
		printf(" lock_mode AUTO-INC");
	} else {
		printf(" unknown lock_mode %lu", lock_get_mode(lock));
	}

	if (lock_get_wait(lock)) {
		printf(" waiting");
	}

	printf("\n");
}						
				
/*************************************************************************
Prints info of a record lock. */

void
lock_rec_print(
/*===========*/
	lock_t*	lock)	/* in: record type lock */
{
	page_t*	page;
	ulint	space;
	ulint	page_no;
	ulint	i;
	ulint	count	= 0;
	ulint   len;
	char    buf[200];
	mtr_t	mtr;

	ut_ad(mutex_own(&kernel_mutex));
	ut_a(lock_get_type(lock) == LOCK_REC);

	space = lock->un_member.rec_lock.space;
 	page_no = lock->un_member.rec_lock.page_no;

	printf("RECORD LOCKS space id %lu page no %lu n bits %lu",
		    space, page_no, lock_rec_get_n_bits(lock));

	printf(" table %s index %s trx id %lu %lu",
		lock->index->table->name, lock->index->name,
		(lock->trx)->id.high, (lock->trx)->id.low);

	if (lock_get_mode(lock) == LOCK_S) {
		printf(" lock mode S");
	} else if (lock_get_mode(lock) == LOCK_X) {
		printf(" lock_mode X");
	} else {
		ut_error;
	}

	if (lock_rec_get_gap(lock)) {
		printf(" gap type lock");
	}

	if (lock_get_wait(lock)) {
		printf(" waiting");
	}

	mtr_start(&mtr);

	printf("\n");

	/* If the page is not in the buffer pool, we cannot load it
	because we have the kernel mutex and ibuf operations would
	break the latching order */
	
	page = buf_page_get_gen(space, page_no, RW_NO_LATCH,
					NULL, BUF_GET_IF_IN_POOL,
					IB__FILE__, __LINE__, &mtr);
	if (page) {
		page = buf_page_get_nowait(space, page_no, RW_S_LATCH, &mtr);
	}
				
	if (page) {
		buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);
	}

	for (i = 0; i < lock_rec_get_n_bits(lock); i++) {

		if (lock_rec_get_nth_bit(lock, i)) {

			printf("Record lock, heap no %lu ", i);

			if (page) {
			  len = rec_sprintf(buf, 190,
				      page_find_rec_with_heap_no(page, i));
			  buf[len] = '\0';
			  printf("%s", buf);
			}

			printf("\n");
			count++;
		}

		if (count >= 3) {
			printf(
    "3 LOCKS PRINTED FOR THIS TRX AND PAGE: SUPPRESSING FURTHER PRINTS\n");
    			goto end_prints;
    		}
	}
end_prints:
	mtr_commit(&mtr);
}						
				
/*************************************************************************
Calculates the number of record lock structs in the record lock hash table. */
static
ulint
lock_get_n_rec_locks(void)
/*======================*/
{
	lock_t*	lock;
	ulint	n_locks	= 0;
	ulint	i;

	ut_ad(mutex_own(&kernel_mutex));

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {

		lock = HASH_GET_FIRST(lock_sys->rec_hash, i);

		while (lock) {
			n_locks++;

			lock = HASH_GET_NEXT(hash, lock);
		}
	}

	return(n_locks);
}
	
/*************************************************************************
Prints info of locks for all transactions. */

void
lock_print_info(void)
/*=================*/
{
	lock_t*	lock;
	trx_t*	trx;
	ulint	space;
	ulint	page_no;
	page_t*	page;
	ibool	load_page_first = TRUE;
	ulint	nth_trx		= 0;
	ulint	nth_lock	= 0;
	ulint	i;
	mtr_t	mtr;

	printf("Trx id counter %lu %lu\n", 
		ut_dulint_get_high(trx_sys->max_trx_id),
		ut_dulint_get_low(trx_sys->max_trx_id));

	printf(
	"Purge done for trx's n:o < %lu %lu undo n:o < %lu %lu\n",
		ut_dulint_get_high(purge_sys->purge_trx_no),
		ut_dulint_get_low(purge_sys->purge_trx_no),
		ut_dulint_get_high(purge_sys->purge_undo_no),
		ut_dulint_get_low(purge_sys->purge_undo_no));
	
	lock_mutex_enter_kernel();

	printf("Total number of lock structs in row lock hash table %lu\n",
						lock_get_n_rec_locks());

	/* First print info on non-active transactions */

	trx = UT_LIST_GET_FIRST(trx_sys->mysql_trx_list);

	while (trx) {
		if (trx->conc_state == TRX_NOT_STARTED) {
		        printf("---");
			trx_print(trx);
		}
			
		trx = UT_LIST_GET_NEXT(mysql_trx_list, trx);
	}

loop:
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	i = 0;

	/* Since we temporarily release the kernel mutex when
	reading a database page in below, variable trx may be
	obsolete now and we must loop through the trx list to
	get probably the same trx, or some other trx. */
	
	while (trx && (i < nth_trx)) {
		trx = UT_LIST_GET_NEXT(trx_list, trx);
		i++;
	}

	if (trx == NULL) {
		lock_mutex_exit_kernel();

		/* lock_validate(); */

		return;
	}

	if (nth_lock == 0) {
	        printf("---");
		trx_print(trx);

	        if (trx->read_view) {
	  	        printf(
       "Trx read view will not see trx with id >= %lu %lu, sees < %lu %lu\n",
		       	ut_dulint_get_high(trx->read_view->low_limit_id),
       			ut_dulint_get_low(trx->read_view->low_limit_id),
       			ut_dulint_get_high(trx->read_view->up_limit_id),
       			ut_dulint_get_low(trx->read_view->up_limit_id));
	        }

		if (trx->que_state == TRX_QUE_LOCK_WAIT) {
			printf(
			"------------------TRX IS WAITING FOR THE LOCK:\n");

			if (lock_get_type(trx->wait_lock) == LOCK_REC) {
				lock_rec_print(trx->wait_lock);
			} else {
				lock_table_print(trx->wait_lock);
			}

			printf(
			"------------------\n");
		}
	}

	if (!srv_print_innodb_lock_monitor) {
	  	nth_trx++;
	  	goto loop;
	}

	i = 0;

	/* Look at the note about the trx loop above why we loop here:
	lock may be an obsolete pointer now. */
	
	lock = UT_LIST_GET_FIRST(trx->trx_locks);
		
	while (lock && (i < nth_lock)) {
		lock = UT_LIST_GET_NEXT(trx_locks, lock);
		i++;
	}

	if (lock == NULL) {
		nth_trx++;
		nth_lock = 0;

		goto loop;
	}

	if (lock_get_type(lock) == LOCK_REC) {
		space = lock->un_member.rec_lock.space;
 		page_no = lock->un_member.rec_lock.page_no;

 		if (load_page_first) {
			lock_mutex_exit_kernel();

			mtr_start(&mtr);
			
			page = buf_page_get_with_no_latch(space, page_no, &mtr);

			mtr_commit(&mtr);

			load_page_first = FALSE;

			lock_mutex_enter_kernel();

			goto loop;
		}
		
		lock_rec_print(lock);
	} else {
		ut_ad(lock_get_type(lock) == LOCK_TABLE);
	
		lock_table_print(lock);
	}

	load_page_first = TRUE;

	nth_lock++;

	if (nth_lock >= 10) {
		printf(
		"10 LOCKS PRINTED FOR THIS TRX: SUPPRESSING FURTHER PRINTS\n");
	
		nth_trx++;
		nth_lock = 0;

		goto loop;
	}

	goto loop;
}

/*************************************************************************
Validates the lock queue on a table. */

ibool
lock_table_queue_validate(
/*======================*/
				/* out: TRUE if ok */
	dict_table_t*	table)	/* in: table */
{
	lock_t*	lock;
	ibool	is_waiting;

	ut_ad(mutex_own(&kernel_mutex));

	is_waiting = FALSE;

	lock = UT_LIST_GET_FIRST(table->locks);

	while (lock) {
		ut_a(((lock->trx)->conc_state == TRX_ACTIVE)
		     || ((lock->trx)->conc_state == TRX_COMMITTED_IN_MEMORY));
	
		if (!lock_get_wait(lock)) {

			ut_a(!is_waiting);
		
			ut_a(!lock_table_other_has_incompatible(lock->trx, 0,
						table, lock_get_mode(lock)));
		} else {
			is_waiting = TRUE;

			ut_a(lock_table_has_to_wait_in_queue(lock));
		}

		lock = UT_LIST_GET_NEXT(un_member.tab_lock.locks, lock);
	}

	return(TRUE);
}

/*************************************************************************
Validates the lock queue on a single record. */

ibool
lock_rec_queue_validate(
/*====================*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: record to look at */
	dict_index_t*	index)	/* in: index, or NULL if not known */
{
	trx_t*	impl_trx;	
	lock_t*	lock;
	ibool	is_waiting;
	
	ut_a(rec);

	lock_mutex_enter_kernel();

	if (page_rec_is_supremum(rec) || page_rec_is_infimum(rec)) {

		lock = lock_rec_get_first(rec);

		while (lock) {
			ut_a(lock->trx->conc_state == TRX_ACTIVE
		     	     || lock->trx->conc_state
						== TRX_COMMITTED_IN_MEMORY);
	
			ut_a(trx_in_trx_list(lock->trx));
			
			if (lock_get_wait(lock)) {
				ut_a(lock_rec_has_to_wait_in_queue(lock));
			}

			if (index) {
				ut_a(lock->index == index);
			}

			lock = lock_rec_get_next(rec, lock);
		}

		lock_mutex_exit_kernel();

	    	return(TRUE);
	}

	if (index && (index->type & DICT_CLUSTERED)) {
	
		impl_trx = lock_clust_rec_some_has_impl(rec, index);

		if (impl_trx && lock_rec_other_has_expl_req(LOCK_S, 0,
						LOCK_WAIT, rec, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X, rec, impl_trx));
		}
	}

	if (index && !(index->type & DICT_CLUSTERED)) {
		
		/* The kernel mutex may get released temporarily in the
		next function call: we have to release lock table mutex
		to obey the latching order */
		
		impl_trx = lock_sec_rec_some_has_impl_off_kernel(rec, index);

		if (impl_trx && lock_rec_other_has_expl_req(LOCK_S, 0,
						LOCK_WAIT, rec, impl_trx)) {

			ut_a(lock_rec_has_expl(LOCK_X, rec, impl_trx));
		}
	}

	is_waiting = FALSE;

	lock = lock_rec_get_first(rec);

	while (lock) {
		ut_a(lock->trx->conc_state == TRX_ACTIVE
		     || lock->trx->conc_state == TRX_COMMITTED_IN_MEMORY);
		ut_a(trx_in_trx_list(lock->trx));
	
		if (index) {
			ut_a(lock->index == index);
		}

		if (!lock_rec_get_gap(lock) && !lock_get_wait(lock)) {

			ut_a(!is_waiting);
		
			if (lock_get_mode(lock) == LOCK_S) {
				ut_a(!lock_rec_other_has_expl_req(LOCK_X,
								0, 0, rec,
								lock->trx));
			} else {
				ut_a(!lock_rec_other_has_expl_req(LOCK_S,
								0, 0, rec,
								lock->trx));
			}

		} else if (lock_get_wait(lock) && !lock_rec_get_gap(lock)) {

			is_waiting = TRUE;
			ut_a(lock_rec_has_to_wait_in_queue(lock));
		}

		lock = lock_rec_get_next(rec, lock);
	}

	lock_mutex_exit_kernel();

	return(TRUE);
}

/*************************************************************************
Validates the record lock queues on a page. */

ibool
lock_rec_validate_page(
/*===================*/
			/* out: TRUE if ok */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	dict_index_t*	index;
	page_t*	page;
	lock_t*	lock;
	rec_t*	rec;
	ulint	nth_lock	= 0;
	ulint	nth_bit		= 0;
	ulint	i;
	mtr_t	mtr;

	ut_ad(!mutex_own(&kernel_mutex));

	mtr_start(&mtr);
	
	page = buf_page_get(space, page_no, RW_X_LATCH, &mtr);
	buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);

	lock_mutex_enter_kernel();
loop:	
	lock = lock_rec_get_first_on_page_addr(space, page_no);

	if (!lock) {
		goto function_exit;
	}

	for (i = 0; i < nth_lock; i++) {

		lock = lock_rec_get_next_on_page(lock);

		if (!lock) {
			goto function_exit;
		}
	}

	ut_a(trx_in_trx_list(lock->trx));
	ut_a(lock->trx->conc_state == TRX_ACTIVE
		     || lock->trx->conc_state == TRX_COMMITTED_IN_MEMORY);
	
	for (i = nth_bit; i < lock_rec_get_n_bits(lock); i++) {

		if (i == 1 || lock_rec_get_nth_bit(lock, i)) {

			index = lock->index;
			rec = page_find_rec_with_heap_no(page, i);

			printf("Validating %lu %lu\n", space, page_no);

			lock_mutex_exit_kernel();

			lock_rec_queue_validate(rec, index);

			lock_mutex_enter_kernel();

			nth_bit = i + 1;

			goto loop;
		}
	}

	nth_bit = 0;
	nth_lock++;

	goto loop;

function_exit:
	lock_mutex_exit_kernel();

	mtr_commit(&mtr);

	return(TRUE);
}						
				
/*************************************************************************
Validates the lock system. */

ibool
lock_validate(void)
/*===============*/
			/* out: TRUE if ok */
{
	lock_t*	lock;
	trx_t*	trx;
	dulint	limit;
	ulint	space;
	ulint	page_no;
	ulint	i;

	lock_mutex_enter_kernel();
	
	trx = UT_LIST_GET_FIRST(trx_sys->trx_list);

	while (trx) {
		lock = UT_LIST_GET_FIRST(trx->trx_locks);
		
		while (lock) {
			if (lock_get_type(lock) == LOCK_TABLE) {
	
				lock_table_queue_validate(
					lock->un_member.tab_lock.table);
			}
	
			lock = UT_LIST_GET_NEXT(trx_locks, lock);
		}
	
		trx = UT_LIST_GET_NEXT(trx_list, trx);
	}

	for (i = 0; i < hash_get_n_cells(lock_sys->rec_hash); i++) {

		limit = ut_dulint_zero;

		for (;;) {
			lock = HASH_GET_FIRST(lock_sys->rec_hash, i);

			while (lock) {
				ut_a(trx_in_trx_list(lock->trx));

				space = lock->un_member.rec_lock.space;
				page_no = lock->un_member.rec_lock.page_no;
		
				if (ut_dulint_cmp(
					ut_dulint_create(space, page_no),
							limit) >= 0) {
					break;
				}

				lock = HASH_GET_NEXT(hash, lock);
			}

			if (!lock) {

				break;
			}
			
			lock_mutex_exit_kernel();

			lock_rec_validate_page(space, page_no);

			lock_mutex_enter_kernel();

			limit = ut_dulint_create(space, page_no + 1);
		}
	}

	lock_mutex_exit_kernel();

	return(TRUE);
}

/*============ RECORD LOCK CHECKS FOR ROW OPERATIONS ====================*/

/*************************************************************************
Checks if locks of other transactions prevent an immediate insert of
a record. If they do, first tests if the query thread should anyway
be suspended for some reason; if not, then puts the transaction and
the query thread to the lock wait state and inserts a waiting request
for a gap x-lock to the lock queue. */

ulint
lock_rec_insert_check_and_lock(
/*===========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record after which to insert */
	dict_index_t*	index,	/* in: index */
	que_thr_t*	thr,	/* in: query thread */
	ibool*		inherit)/* out: set to TRUE if the new inserted
				record maybe should inherit LOCK_GAP type
				locks from the successor record */
{
	rec_t*	next_rec;
	trx_t*	trx;
	lock_t*	lock;
	ulint	err;

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_ad(rec);

	trx = thr_get_trx(thr);
	next_rec = page_rec_get_next(rec);

	*inherit = FALSE;

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	lock = lock_rec_get_first(next_rec);

	if (lock == NULL) {
		/* We optimize CPU time usage in the simplest case */

		lock_mutex_exit_kernel();

		if (!(index->type & DICT_CLUSTERED)) {

			/* Update the page max trx id field */
			page_update_max_trx_id(buf_frame_align(rec),
							thr_get_trx(thr)->id);
		}
		
		return(DB_SUCCESS);
	}

	*inherit = TRUE;

	/* If another transaction has an explicit lock request, gap or not,
	waiting or granted, on the successor, the insert has to wait */

	if (lock_rec_other_has_expl_req(LOCK_S, LOCK_GAP, LOCK_WAIT, next_rec,
								trx)) {
		err = lock_rec_enqueue_waiting(LOCK_X | LOCK_GAP, next_rec,
								index, thr);
	} else {
		err = DB_SUCCESS;
	}

	lock_mutex_exit_kernel();

	if (!(index->type & DICT_CLUSTERED) && (err == DB_SUCCESS)) {

		/* Update the page max trx id field */
		page_update_max_trx_id(buf_frame_align(rec),
							thr_get_trx(thr)->id);
	}
	
	ut_ad(lock_rec_queue_validate(next_rec, index));

	return(err);
}

/*************************************************************************
If a transaction has an implicit x-lock on a record, but no explicit x-lock
set on the record, sets one for it. NOTE that in the case of a secondary
index, the kernel mutex may get temporarily released. */
static
void
lock_rec_convert_impl_to_expl(
/*==========================*/
	rec_t*		rec,	/* in: user record on page */
	dict_index_t*	index)	/* in: index of record */
{
	trx_t*	impl_trx;

	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(page_rec_is_user_rec(rec));

	if (index->type & DICT_CLUSTERED) {
		impl_trx = lock_clust_rec_some_has_impl(rec, index);
	} else {
		impl_trx = lock_sec_rec_some_has_impl_off_kernel(rec, index);
	}

	if (impl_trx) {
		/* If the transaction has no explicit x-lock set on the
		record, set one for it */

		if (!lock_rec_has_expl(LOCK_X, rec, impl_trx)) {

			lock_rec_add_to_queue(LOCK_REC | LOCK_X, rec, index,
								impl_trx);
		}
	}
}

/*************************************************************************
Checks if locks of other transactions prevent an immediate modify (update,
delete mark, or delete unmark) of a clustered index record. If they do,
first tests if the query thread should anyway be suspended for some
reason; if not, then puts the transaction and the query thread to the
lock wait state and inserts a waiting request for a record x-lock to the
lock queue. */

ulint
lock_clust_rec_modify_check_and_lock(
/*=================================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record which should be modified */
	dict_index_t*	index,	/* in: clustered index */
	que_thr_t*	thr)	/* in: query thread */
{
	trx_t*	trx;
	ulint	err;
	
	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_ad(index->type & DICT_CLUSTERED);

	trx = thr_get_trx(thr);

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	/* If a transaction has no explicit x-lock set on the record, set one
	for it */

	lock_rec_convert_impl_to_expl(rec, index);

	err = lock_rec_lock(TRUE, LOCK_X, rec, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(rec, index));

	return(err);
}

/*************************************************************************
Checks if locks of other transactions prevent an immediate modify (delete
mark or delete unmark) of a secondary index record. */

ulint
lock_sec_rec_modify_check_and_lock(
/*===============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: record which should be modified;
				NOTE: as this is a secondary index, we
				always have to modify the clustered index
				record first: see the comment below */
	dict_index_t*	index,	/* in: secondary index */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;
	
	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	ut_ad(!(index->type & DICT_CLUSTERED));

	/* Another transaction cannot have an implicit lock on the record,
	because when we come here, we already have modified the clustered
	index record, and this would not have been possible if another active
	transaction had modified this secondary index record. */

	lock_mutex_enter_kernel();

	ut_ad(lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));

	err = lock_rec_lock(TRUE, LOCK_X, rec, index, thr);

	lock_mutex_exit_kernel();
	
	ut_ad(lock_rec_queue_validate(rec, index));

	if (err == DB_SUCCESS) {
		/* Update the page max trx id field */

		page_update_max_trx_id(buf_frame_align(rec),
							thr_get_trx(thr)->id);
	}

	return(err);
}

/*************************************************************************
Like the counterpart for a clustered index below, but now we read a
secondary index record. */

ulint
lock_sec_rec_read_check_and_lock(
/*=============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: user record or page supremum record
				which should be read or passed over by a read
				cursor */
	dict_index_t*	index,	/* in: secondary index */
	ulint		mode,	/* in: mode of the lock which the read cursor
				should set on records: LOCK_S or LOCK_X; the
				latter is possible in SELECT FOR UPDATE */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(!(index->type & DICT_CLUSTERED));
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));

	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	lock_mutex_enter_kernel();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));

	/* Some transaction may have an implicit x-lock on the record only
	if the max trx id for the page >= min trx id for the trx list or a
	database recovery is running. */

	if (((ut_dulint_cmp(page_get_max_trx_id(buf_frame_align(rec)),
					trx_list_get_min_trx_id()) >= 0)
	     		|| recv_recovery_is_on())
	     && !page_rec_is_supremum(rec)) {

 		lock_rec_convert_impl_to_expl(rec, index);
	}

	err = lock_rec_lock(FALSE, mode, rec, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(rec, index));

	return(err);
}

/*************************************************************************
Checks if locks of other transactions prevent an immediate read, or passing
over by a read cursor, of a clustered index record. If they do, first tests
if the query thread should anyway be suspended for some reason; if not, then
puts the transaction and the query thread to the lock wait state and inserts a
waiting request for a record lock to the lock queue. Sets the requested mode
lock on the record. */

ulint
lock_clust_rec_read_check_and_lock(
/*===============================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DEADLOCK, or DB_QUE_THR_SUSPENDED */
	ulint		flags,	/* in: if BTR_NO_LOCKING_FLAG bit is set,
				does nothing */
	rec_t*		rec,	/* in: user record or page supremum record
				which should be read or passed over by a read
				cursor */
	dict_index_t*	index,	/* in: clustered index */
	ulint		mode,	/* in: mode of the lock which the read cursor
				should set on records: LOCK_S or LOCK_X; the
				latter is possible in SELECT FOR UPDATE */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(page_rec_is_user_rec(rec) || page_rec_is_supremum(rec));
	
	if (flags & BTR_NO_LOCKING_FLAG) {

		return(DB_SUCCESS);
	}

	lock_mutex_enter_kernel();

	ut_ad(mode != LOCK_X
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IX));
	ut_ad(mode != LOCK_S
	      || lock_table_has(thr_get_trx(thr), index->table, LOCK_IS));
	
	if (!page_rec_is_supremum(rec)) {
	      
		lock_rec_convert_impl_to_expl(rec, index);
	}

	err = lock_rec_lock(FALSE, mode, rec, index, thr);

	lock_mutex_exit_kernel();

	ut_ad(lock_rec_queue_validate(rec, index));
	
	return(err);
}
