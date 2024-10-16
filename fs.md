# File System
[MIT6.1810 file system Page](https://pdos.csail.mit.edu/6.S081/2022/labs/fs.html)

This lab introduces you to file system API and the **LOG** mechanism.

## Large Files
This task is to modify the file system so that it can support larger files.
The big idea is **Add an Indirection**

First, we modify the file index structure in `fs.h`
```c
// * Modify the structure
// #define NDIRECT 12
// #define NINDIRECT (BSIZE / sizeof(uint))
// #define MAXFILE (NDIRECT + NINDIRECT)
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT NINDIRECT * NINDIRECT // - 256 * 256
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT) // - add doubly-indirect
```

Don't forget to modify `addr` in `inode` of `file.h`
```c
struct dinode {
  ...
  // * modify addrs
  // uint addrs[NDIRECT+1];
  uint addrs[NDIRECT+2];
};
struct inode {
  ...
  // * modify addrs
  // uint addrs[NDIRECT+1];
  uint addrs[NDIRECT+2];
};
```

Then in `bmap`, add the logic of doubly-indirect
```c
static uint
bmap(struct inode *ip, uint bn)
{
    ...
  if(bn < NINDIRECT){ ... }
  // * Add a new check
  bn -= NINDIRECT;
  int ioffset = bn / NINDIRECT; // offset in the first-level block
  int boffset = bn - ioffset * NINDIRECT; // offset in the second-level block
  // now bn = 
  if(bn < NDINDIRECT){
      if((addr = ip->addrs[NDIRECT+1]) == 0){
          addr = balloc(ip->dev);
          if(addr == 0)
              return 0;
          ip->addrs[NDIRECT+1] = addr;
      }
      bp = bread(ip->dev,addr); // loading indirect block
      a = (uint*) bp->data;
      if((addr = a[ioffset]) == 0){
          addr = balloc(ip->dev);
          if(addr){
              a[ioffset] = addr;
              log_write(bp);
          }
      }
      brelse(bp);
      bp = bread(ip->dev,addr); // loading the doubly-indirect block
      a = (uint*) bp->data;
      if((addr = a[boffset]) == 0){
          addr = balloc(ip->dev);
          if(addr){
              a[boffset] = addr;
              log_write(bp);
          }
      }
      brelse(bp);
      return addr;
  }
  panic("bmap: out of range");
}
```

## Symbolic links
We will implement a `symlink` system call, which creates a new file that contains the path to an existing file. 

First, register the syscall like [lab:syscall](./syscall.md)
Then follow the lab page:
> Add a new file type (T_SYMLINK) to kernel/stat.h to represent a symbolic link.
> Add a new flag to kernel/fcntl.h, (O_NOFOLLOW), that can be used with the open system call.

In `sysfile.c`, implement the `sys_symlink`:
```c
uint64
sys_symlink(void){
    // - parse the parameters
  char target[MAXPATH], path[MAXPATH];

  if(argstr(0,target,MAXPATH) < 0 || argstr(1,path,MAXPATH) < 0)
    return -1;
  
  struct inode *ip;
  // create a node
  begin_op();
  if((ip = create(path, T_SYMLINK, 0, 0)) == 0){
    end_op();
    return -1;
  }
  // write the target to the node
  if(writei(ip, 0, (uint64)target, 0, sizeof(target)) < sizeof(target)){
    iunlockput(ip);
    end_op();
  }
  iunlockput(ip);
  end_op();
  return 0;
}
```

Modify `sys_open` to follow the symbol links:
```c
uint64
sys_open(void)
{
  ...
  if(omode & O_CREATE){ ... } 
  else {
    ...
    if(ip->type == T_DIR && omode != O_RDONLY){ ... }
    // * A symbol link
    if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)){
      int depth = 0;
      if((ip = follow(ip,&depth)) == 0){
        end_op();
        return -1;
      }
      ilock(ip);
    }
  }
  ...
}
```
Then implement the `follow` function:
```c
/*
  follow a symbolic link recursively
  caller must hold the lock of link
 */
struct inode* follow(struct inode* link,int* depth)
{
  if(((*depth)++) >= 10){
    printf("follow: loop link\n");
    goto fail;
  }
  char path[MAXPATH];
  // struct inode *ip;
  if(readi(link, 0, (uint64)path, 0, MAXPATH) != MAXPATH){
    printf("symlink has no target\n");
    goto fail;
  }
  // - now get the new path,try open the path
  iunlockput(link);
  if((link = namei(path)) == 0){
    // ! not found
    printf("follow: not exist\n");
    return 0;
  }
  if(link->type != T_SYMLINK){
    // printf("get %s,type: %d,nlink:%d,nref:%d\n",path,ip->type,ip->nlink,ip->ref);
    return link;
  }
  ilock(link);
  return follow(link, depth);

fail:
  iunlockput(link);
  return 0;
}
```
